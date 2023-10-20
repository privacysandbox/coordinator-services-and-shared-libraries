/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "pbs/pbs_server/src/pbs_instance/pbs_instance_multi_partition.h"

#include <gtest/gtest.h>

#include <stdlib.h>

#include <filesystem>
#include <string>

#include "absl/strings/str_join.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/config_provider/src/env_config_provider.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/lease_manager_interface.h"
#include "core/tcp_traffic_forwarder/mock/mock_traffic_forwarder.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/logging_utils.h"
#include "core/token_provider_cache/mock/token_provider_cache_dummy.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/leasable_lock/mock/mock_leasable_lock.h"
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/local/local_dependency_factory.h"
#include "pbs/pbs_server/src/pbs_instance/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::EnvConfigProvider;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetTransactionStatusRequest;
using google::scp::core::GetTransactionStatusResponse;
using google::scp::core::HttpClient;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpClientOptions;
using google::scp::core::LeasableLockInterface;
using google::scp::core::LeaseInfo;
using google::scp::core::PartitionId;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TimeDuration;
using google::scp::core::TokenProviderCacheInterface;
using google::scp::core::TransactionRequest;
using google::scp::core::TransactionResponse;
using google::scp::core::common::RetryStrategyOptions;
using google::scp::core::common::RetryStrategyType;
using google::scp::core::common::TimeProvider;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::tcp_traffic_forwarder::mock::MockTCPTrafficForwarder;
using google::scp::core::test::ResultIs;
using google::scp::core::test::TestLoggingUtils;
using google::scp::core::test::WaitUntil;
using google::scp::core::token_provider_cache::mock::DummyTokenProviderCache;
using google::scp::pbs::PBSInstanceMultiPartition;
using google::scp::pbs::leasable_lock::mock::MockLeasableLock;
using std::atomic;
using std::cerr;
using std::cout;
using std::endl;
using std::make_shared;
using std::mutex;
using std::optional;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unique_lock;
using std::vector;
using std::chrono::milliseconds;
using std::chrono::minutes;
using std::chrono::seconds;
using std::filesystem::create_directory;
using std::filesystem::exists;
using std::filesystem::path;
using std::filesystem::remove_all;
using std::this_thread::sleep_for;

static constexpr char kDefaultBucketName[] = "bucket";

namespace google::scp::pbs::test {

class PBSInstanceMultiPartitionTest : public ::testing::Test {
 protected:
  PBSInstanceMultiPartitionTest() {
    TestLoggingUtils::EnableLogOutputToConsole();
    config_provider_ = make_shared<EnvConfigProvider>();
    async_executor_ = make_shared<AsyncExecutor>(
        5 /* thread pool size */, 100000 /* queue size */,
        false /* drop_tasks_on_stop */);
    HttpClientOptions client_options(
        RetryStrategyOptions(RetryStrategyType::Exponential,
                             50 /* delay in ms */, 5 /* num retries */),
        20 /* max connections per host */, 5 /* read timeout in sec */);
    http_client_ = make_shared<HttpClient>(async_executor_, client_options);

    EXPECT_SUCCESS(async_executor_->Init());
    EXPECT_SUCCESS(async_executor_->Run());

    EXPECT_SUCCESS(http_client_->Init());
    EXPECT_SUCCESS(http_client_->Run());

    partition_ids_ = {PartitionId{0, 1}, PartitionId{0, 2}, PartitionId{0, 3}};
    vnode_ids_ = {Uuid{UINT64_MAX, 1}, Uuid{UINT64_MAX, 2},
                  Uuid{UINT64_MAX, 3}};

    // Appending the Vnode IDs to the same table.
    vector<Uuid> row_ids_to_create;
    row_ids_to_create.insert(row_ids_to_create.end(), partition_ids_.begin(),
                             partition_ids_.end());
    row_ids_to_create.insert(row_ids_to_create.end(), vnode_ids_.begin(),
                             vnode_ids_.end());

    platform_dependency_factory_ = make_shared<LocalDependencyFactory>(
        config_provider_, row_ids_to_create);
    EXPECT_SUCCESS(platform_dependency_factory_->Init());
  }

  ~PBSInstanceMultiPartitionTest() {
    EXPECT_SUCCESS(http_client_->Stop());
    EXPECT_SUCCESS(async_executor_->Stop());
  }

  shared_ptr<ConfigProviderInterface> config_provider_;
  shared_ptr<AsyncExecutorInterface> async_executor_;
  shared_ptr<HttpClientInterface> http_client_;
  shared_ptr<CloudPlatformDependencyFactoryInterface>
      platform_dependency_factory_;
  vector<PartitionId> partition_ids_;
  vector<Uuid> vnode_ids_;
};

static bool InitiateTransactionOnPBSClient(
    shared_ptr<PrivacyBudgetServiceClientInterface>& pbs_client) {
  // Ensure atleast a transaction request goes through. Create a sample
  // request
  // and send it to PBS endpoint.
  atomic<bool> callback_received = false;
  atomic<bool> succeeded = false;
  AsyncContext<ConsumeBudgetTransactionRequest,
               ConsumeBudgetTransactionResponse>
      context;
  context.request = make_shared<ConsumeBudgetTransactionRequest>();
  context.request->transaction_id = Uuid::GenerateUuid();
  context.request->transaction_secret = make_shared<string>("secret");
  context.request->budget_keys = make_shared<vector<ConsumeBudgetMetadata>>();
  context.request->budget_keys->push_back(
      {make_shared<string>("key"), 1 /* token */, 1 /* time bucket */});
  context.callback = [&callback_received, &succeeded](auto& context) {
    if (context.result.Successful()) {
      succeeded = true;
    }
    callback_received = true;
  };
  EXPECT_SUCCESS(pbs_client->InitiateConsumeBudgetTransaction(context));
  WaitUntil([&callback_received]() { return callback_received.load(); },
            seconds(20));
  return succeeded;
}

static void SetPBSConfigurationEnvVarsForMultiInstance(
    const vector<PartitionId>& partition_ids, const vector<Uuid> vnode_ids) {
  // Set configurations.
  bool should_overwrite = true;
  setenv(kAsyncExecutorQueueSize, "1000000", should_overwrite);
  setenv(kAsyncExecutorThreadsCount, "20", should_overwrite);
  setenv(kIOAsyncExecutorQueueSize, "1000000", should_overwrite);
  setenv(kIOAsyncExecutorThreadsCount, "20", should_overwrite);
  setenv(kTransactionManagerCapacity, "1000000", should_overwrite);
  setenv(kJournalServiceBucketName, kDefaultBucketName, should_overwrite);
  setenv(kJournalServicePartitionName, "00000000-0000-0000-0000-000000000000",
         should_overwrite);
  setenv(kPrivacyBudgetServiceHostAddress, "localhost", should_overwrite);
  setenv(kRemotePrivacyBudgetServiceCloudServiceRegion, "us-east-1",
         should_overwrite);
  setenv(kRemotePrivacyBudgetServiceAuthServiceEndpoint, "https://auth.com",
         should_overwrite);
  setenv(kRemotePrivacyBudgetServiceClaimedIdentity, "remote-id",
         should_overwrite);
  setenv(kRemotePrivacyBudgetServiceAssumeRoleArn, "arn", should_overwrite);
  setenv(kRemotePrivacyBudgetServiceAssumeRoleExternalId, "id",
         should_overwrite);
  setenv(kRemotePrivacyBudgetServiceHostAddress, "https://remote.com",
         should_overwrite);
  setenv(kAuthServiceEndpoint, "https://auth.com", should_overwrite);
  setenv(core::kCloudServiceRegion, "region", should_overwrite);
  setenv(kServiceMetricsNamespace, "ns", should_overwrite);
  setenv(kTotalHttp2ServerThreadsCount, "10", should_overwrite);
  setenv(kPBSPartitionLockTableNameConfigName, "partition_lock_table",
         should_overwrite);
  setenv(core::kHTTPServerRequestRoutingEnabled, "true", should_overwrite);

  struct UUIDToStringFormatter {
    void operator()(std::string* out, PartitionId id) const {
      out->append(ToString(id));
    }
  };

  auto partition_ids_string =
      absl::StrJoin(partition_ids, ",", UUIDToStringFormatter());

  setenv(kPBSPartitionIdList, partition_ids_string.c_str(), should_overwrite);

  auto vnode_ids_string =
      absl::StrJoin(vnode_ids, ",", UUIDToStringFormatter());

  setenv(kPBSVirtualNodeIdList, vnode_ids_string.c_str(), should_overwrite);

  setenv(kBudgetKeyTableName, "budget", should_overwrite);
}

static void RecreateDir(path partition_dir_path) {
  if (!exists(kDefaultBucketName)) {
    create_directory(kDefaultBucketName);
  }
}

static void RemoveDir(path partition_dir_path) {
  // Remove log folder
  remove_all(kDefaultBucketName);
}

static void CreatePBSClientsForPBSServerEndpoint(
    size_t num_clients, string endpoint_url,
    shared_ptr<HttpClientInterface> http_client,
    vector<shared_ptr<PrivacyBudgetServiceClientInterface>>& pbs_clients) {
  auto dummy_token_provider_cache = make_shared<DummyTokenProviderCache>();
  for (int i = 0; i < num_clients; i++) {
    auto client_reporting_origin = ToString(Uuid::GenerateUuid());
    pbs_clients.push_back(make_shared<PrivacyBudgetServiceClient>(
        client_reporting_origin, endpoint_url, http_client,
        dummy_token_provider_cache));
  }
}

static void PumpRequests(shared_ptr<HttpClientInterface> http_client,
                         atomic<bool>& stop, vector<string> endpoint_uris,
                         size_t clients_per_endpoint) {
  vector<shared_ptr<PrivacyBudgetServiceClientInterface>> pbs_clients;

  for (const auto& endpoint_uri : endpoint_uris) {
    CreatePBSClientsForPBSServerEndpoint(clients_per_endpoint, endpoint_uri,
                                         http_client, pbs_clients);
  }

  // Start
  for (auto& pbs_client : pbs_clients) {
    EXPECT_SUCCESS(pbs_client->Init());
  }
  for (auto& pbs_client : pbs_clients) {
    EXPECT_SUCCESS(pbs_client->Run());
  }

  /* initialize random seed: */
  auto seed = static_cast<uint32_t>(
      TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks());

  size_t requests_succeeded = 0;
  size_t requests_timed_out = 0;
  for (int i = 0; !stop; i++) {
    // Choose pbs_client
    auto rand_number = rand_r(&seed) % pbs_clients.size();
    try {
      if (InitiateTransactionOnPBSClient(pbs_clients[rand_number])) {
        requests_succeeded++;
      } else {
        requests_timed_out++;
      }
      if (i % 100 == 0) {
        cout << "Requests succeeded so far " << requests_succeeded << endl;
        cout << "Requests timed out so far " << requests_timed_out << endl;
      }
    } catch (...) {
      requests_timed_out++;
    }
  }

  // Stop
  for (auto& pbs_client : pbs_clients) {
    EXPECT_SUCCESS(pbs_client->Stop());
  }

  cout << "Total Requests succeeded " << requests_succeeded << endl;
  cout << "Total Requests timed out " << requests_timed_out << endl;
}

TEST_F(PBSInstanceMultiPartitionTest,
       MultiInstanceThreePartitionsWithTrafficGoesThrough) {
  core::common::GlobalLogger::SetGlobalLogLevels(
      {core::LogLevel::kError, core::LogLevel::kInfo, core::LogLevel::kDebug});

  // Recreate the partition's log directory directory.
  for (const auto& partition_id : partition_ids_) {
    RecreateDir(ToString(partition_id));
  }

  SetPBSConfigurationEnvVarsForMultiInstance(partition_ids_, vnode_ids_);

  // Run
  PBSInstanceMultiPartition pbs_instance1(config_provider_,
                                          platform_dependency_factory_);
  PBSInstanceMultiPartition pbs_instance2(config_provider_,
                                          platform_dependency_factory_);
  PBSInstanceMultiPartition pbs_instance3(config_provider_,
                                          platform_dependency_factory_);

  bool should_overwrite = true;
  setenv(kPrivacyBudgetServiceHostPort, "8001", should_overwrite);
  setenv(kPrivacyBudgetServiceExternalExposedHostPort, "8001",
         should_overwrite);
  setenv(kPrivacyBudgetServiceHealthPort, "9001", should_overwrite);
  EXPECT_SUCCESS(pbs_instance1.Init());

  setenv(kPrivacyBudgetServiceHostPort, "8002", should_overwrite);
  setenv(kPrivacyBudgetServiceExternalExposedHostPort, "8002",
         should_overwrite);
  setenv(kPrivacyBudgetServiceHealthPort, "9002", should_overwrite);
  EXPECT_SUCCESS(pbs_instance2.Init());

  setenv(kPrivacyBudgetServiceHostPort, "8003", should_overwrite);
  setenv(kPrivacyBudgetServiceExternalExposedHostPort, "8003",
         should_overwrite);
  setenv(kPrivacyBudgetServiceHealthPort, "9003", should_overwrite);
  EXPECT_SUCCESS(pbs_instance3.Init());

  EXPECT_SUCCESS(pbs_instance1.Run());
  EXPECT_SUCCESS(pbs_instance2.Run());
  EXPECT_SUCCESS(pbs_instance3.Run());

  // Wait for the lease to be obtained and partitions to be loaded.
  sleep_for(seconds(15));

  // Start traffic
  atomic<bool> stop_request_pumper = false;
  thread request_pumper([http_client = http_client_, &stop_request_pumper]() {
    PumpRequests(http_client, stop_request_pumper,
                 {"http://localhost:8001", "http://localhost:8002",
                  "http://localhost:8003"},
                 5);
  });

  // Wait for some requests to flow in.
  sleep_for(seconds(15));

  // Stop instance 1
  std::cerr << "PBS Instance 1 stopping" << std::endl;
  EXPECT_SUCCESS(pbs_instance1.Stop());

  sleep_for(seconds(15));

  // Stop instance 2
  std::cerr << "PBS Instance 2 stopping" << std::endl;
  EXPECT_SUCCESS(pbs_instance2.Stop());

  sleep_for(seconds(15));

  // Stop instance 3
  std::cerr << "PBS Instance 3 stopping" << std::endl;
  EXPECT_SUCCESS(pbs_instance3.Stop());

  // Remove partition's log directories.
  for (const auto& partition_id : partition_ids_) {
    RemoveDir(ToString(partition_id));
  }

  stop_request_pumper = true;
  request_pumper.join();
}
}  // namespace google::scp::pbs::test
