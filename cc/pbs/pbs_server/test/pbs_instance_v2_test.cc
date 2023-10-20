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

#include "pbs/pbs_server/src/pbs_instance/pbs_instance_v2.h"

#include <gtest/gtest.h>

#include <stdlib.h>

#include <filesystem>
#include <string>

#include "core/async_executor/src/async_executor.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/config_provider/src/env_config_provider.h"
#include "core/http2_client/src/http2_client.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/lease_manager_interface.h"
#include "core/lease_manager/src/lease_manager.h"
#include "core/tcp_traffic_forwarder/mock/mock_traffic_forwarder.h"
#include "core/test/utils/conditional_wait.h"
#include "core/test/utils/logging_utils.h"
#include "core/token_provider_cache/mock/token_provider_cache_dummy.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/leasable_lock/mock/mock_leasable_lock.h"
#include "pbs/pbs_client/src/pbs_client.h"
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/local/local_dependency_factory.h"
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
using google::scp::core::LeaseManager;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TimeDuration;
using google::scp::core::TokenProviderCacheInterface;
using google::scp::core::TransactionRequest;
using google::scp::core::TransactionResponse;
using google::scp::core::common::RetryStrategyOptions;
using google::scp::core::common::RetryStrategyType;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::tcp_traffic_forwarder::mock::MockTCPTrafficForwarder;
using google::scp::core::test::ResultIs;
using google::scp::core::test::TestLoggingUtils;
using google::scp::core::test::WaitUntil;
using google::scp::core::token_provider_cache::mock::DummyTokenProviderCache;
using google::scp::pbs::PBSInstanceV2;
using google::scp::pbs::leasable_lock::mock::MockLeasableLock;
using std::atomic;
using std::make_shared;
using std::mutex;
using std::optional;
using std::shared_ptr;
using std::string;
using std::thread;
using std::unique_lock;
using std::vector;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::filesystem::create_directory;
using std::filesystem::exists;
using std::filesystem::path;
using std::filesystem::remove_all;
using std::this_thread::sleep_for;

static constexpr char kDefaultBucketName[] = "bucket";

namespace google::scp::pbs::test {

class PBSV2InstanceTest : public ::testing::Test {
 protected:
  PBSV2InstanceTest() {
    config_provider_ = make_shared<EnvConfigProvider>();
    async_executor_ = make_shared<AsyncExecutor>(5 /* thread pool size */,
                                                 100000 /* queue size */,
                                                 true /* drop_tasks_on_stop */);
    HttpClientOptions client_options(
        RetryStrategyOptions(RetryStrategyType::Exponential,
                             300 /* delay in ms */, 6 /* num retries */),
        10 /* max connections per host */, 30 /* read timeout in sec */);
    http_client_ = make_shared<HttpClient>(async_executor_, client_options);

    EXPECT_SUCCESS(async_executor_->Init());
    EXPECT_SUCCESS(async_executor_->Run());

    EXPECT_SUCCESS(http_client_->Init());
    EXPECT_SUCCESS(http_client_->Run());

    platform_dependency_factory_ =
        make_shared<LocalDependencyFactory>(config_provider_);
    EXPECT_SUCCESS(platform_dependency_factory_->Init());

    PrepareLogDirectories();
  }

  ~PBSV2InstanceTest() {
    EXPECT_SUCCESS(http_client_->Stop());
    EXPECT_SUCCESS(async_executor_->Stop());
    RemoveLogDirectories();
  }

  void PrepareLogDirectories() {
    if (!exists(kDefaultBucketName)) {
      create_directory(kDefaultBucketName);
    }
  }

  void RemoveLogDirectories() { remove_all(kDefaultBucketName); }

  shared_ptr<ConfigProviderInterface> config_provider_;
  shared_ptr<AsyncExecutorInterface> async_executor_;
  shared_ptr<HttpClientInterface> http_client_;
  shared_ptr<CloudPlatformDependencyFactoryInterface>
      platform_dependency_factory_;
};

static Uuid InitiateTransactionOnPBSClient(
    shared_ptr<PrivacyBudgetServiceClient>& pbs_client) {
  // Ensure atleast a transaction request goes through. Create a sample
  // request
  // and send it to PBS endpoint.
  atomic<bool> callback_received = false;
  AsyncContext<ConsumeBudgetTransactionRequest,
               ConsumeBudgetTransactionResponse>
      context;
  context.request = make_shared<ConsumeBudgetTransactionRequest>();
  context.request->transaction_id = Uuid::GenerateUuid();
  context.request->transaction_secret = make_shared<string>("secret");
  context.request->budget_keys = make_shared<vector<ConsumeBudgetMetadata>>();
  context.request->budget_keys->push_back(
      {make_shared<string>("key"), 1 /* token */, 1 /* time bucket */});
  context.callback = [&callback_received](auto& context) {
    EXPECT_SUCCESS(context.result);
    callback_received = true;
  };
  EXPECT_SUCCESS(pbs_client->InitiateConsumeBudgetTransaction(context));
  WaitUntil([&callback_received]() { return callback_received.load(); },
            seconds(20));
  return context.request->transaction_id;
}

static void GetTransactionStatusOnPBSClient(
    shared_ptr<PrivacyBudgetServiceClient>& pbs_client, Uuid transaction_id) {
  atomic<bool> callback_received = false;
  AsyncContext<GetTransactionStatusRequest, GetTransactionStatusResponse>
      context;
  context.request = make_shared<GetTransactionStatusRequest>();
  context.request->transaction_id = transaction_id;
  context.request->transaction_secret = make_shared<string>("secret");
  context.request->transaction_origin = make_shared<string>("origin");
  context.callback = [&callback_received](auto& context) {
    EXPECT_SUCCESS(context.result);
    callback_received = true;
  };
  EXPECT_SUCCESS(pbs_client->GetTransactionStatus(context));
  WaitUntil([&callback_received]() { return callback_received.load(); },
            seconds(20));
}

TEST_F(PBSV2InstanceTest, InitRunStopWithASuccessfulTransactionRequest) {
  // Set configurations.
  bool should_overwrite = true;
  setenv(kAsyncExecutorQueueSize, "10000", should_overwrite);
  setenv(kAsyncExecutorThreadsCount, "10", should_overwrite);
  setenv(kIOAsyncExecutorQueueSize, "10000", should_overwrite);
  setenv(kIOAsyncExecutorThreadsCount, "10", should_overwrite);
  setenv(kTransactionManagerCapacity, "10000", should_overwrite);
  setenv(kJournalServiceBucketName, kDefaultBucketName, should_overwrite);
  setenv(kJournalServicePartitionName, "00000000-0000-0000-0000-000000000000",
         should_overwrite);
  setenv(kPrivacyBudgetServiceHostAddress, "localhost", should_overwrite);
  setenv(kPrivacyBudgetServiceHostPort, "8000", should_overwrite);
  setenv(kPrivacyBudgetServiceHealthPort, "8001", should_overwrite);
  setenv(kPrivacyBudgetServiceExternalExposedHostPort, "8000",
         should_overwrite);
  setenv(kRemotePrivacyBudgetServiceCloudServiceRegion, "region",
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

  // Run
  PBSInstanceV2 pbs_instance(config_provider_, platform_dependency_factory_);
  EXPECT_SUCCESS(pbs_instance.Init());
  EXPECT_SUCCESS(pbs_instance.Run());

  // Wait for the lease to be obtained and partition to be loaded.
  sleep_for(seconds(10));

  auto dummy_token_provider_cache = make_shared<DummyTokenProviderCache>();
  auto pbs_client = make_shared<PrivacyBudgetServiceClient>(
      "reporting_origin", "http://localhost:8000", http_client_,
      dummy_token_provider_cache);
  EXPECT_SUCCESS(pbs_client->Init());
  EXPECT_SUCCESS(pbs_client->Run());

  // Send one BEGIN request to PBS.
  InitiateTransactionOnPBSClient(pbs_client);

  // Stop.
  EXPECT_SUCCESS(pbs_client->Stop());
  EXPECT_SUCCESS(pbs_instance.Stop());
}

static void SetPBSConfigurationEnvVarsForMultiInstance() {
  // Set configurations.
  bool should_overwrite = true;
  setenv(kAsyncExecutorQueueSize, "10000", should_overwrite);
  setenv(kAsyncExecutorThreadsCount, "10", should_overwrite);
  setenv(kIOAsyncExecutorQueueSize, "10000", should_overwrite);
  setenv(kIOAsyncExecutorThreadsCount, "10", should_overwrite);
  setenv(kTransactionManagerCapacity, "10000", should_overwrite);
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
}

TEST_F(PBSV2InstanceTest,
       MultiInstanceTrafficRoutesToInstanceHoldingPartition) {
  // The goal of this test is to determine if 3 PBSInstance setup is working as
  // expected i.e. transactions are able to go through despite which VM they are
  // sent to.
  // Recreate the partition's log directory directory.

  SetPBSConfigurationEnvVarsForMultiInstance();

  // Run
  PBSInstanceV2 pbs_instance1(config_provider_, platform_dependency_factory_);
  PBSInstanceV2 pbs_instance2(config_provider_, platform_dependency_factory_);
  PBSInstanceV2 pbs_instance3(config_provider_, platform_dependency_factory_);

  bool should_overwrite = true;
  setenv(kPrivacyBudgetServiceHostPort, "8000", should_overwrite);
  setenv(kPrivacyBudgetServiceExternalExposedHostPort, "8000",
         should_overwrite);
  setenv(kPrivacyBudgetServiceHealthPort, "9000", should_overwrite);
  EXPECT_SUCCESS(pbs_instance1.Init());

  setenv(kPrivacyBudgetServiceHostPort, "8001", should_overwrite);
  setenv(kPrivacyBudgetServiceExternalExposedHostPort, "8001",
         should_overwrite);
  setenv(kPrivacyBudgetServiceHealthPort, "9001", should_overwrite);
  EXPECT_SUCCESS(pbs_instance2.Init());

  setenv(kPrivacyBudgetServiceHostPort, "8002", should_overwrite);
  setenv(kPrivacyBudgetServiceExternalExposedHostPort, "8002",
         should_overwrite);
  setenv(kPrivacyBudgetServiceHealthPort, "9002", should_overwrite);
  EXPECT_SUCCESS(pbs_instance3.Init());

  EXPECT_SUCCESS(pbs_instance1.Run());
  EXPECT_SUCCESS(pbs_instance2.Run());
  EXPECT_SUCCESS(pbs_instance3.Run());

  auto dummy_token_provider_cache = make_shared<DummyTokenProviderCache>();
  auto pbs_client1 = make_shared<PrivacyBudgetServiceClient>(
      "reporting_origin", "http://localhost:8000", http_client_,
      dummy_token_provider_cache);
  auto pbs_client2 = make_shared<PrivacyBudgetServiceClient>(
      "reporting_origin", "http://localhost:8001", http_client_,
      dummy_token_provider_cache);
  auto pbs_client3 = make_shared<PrivacyBudgetServiceClient>(
      "reporting_origin", "http://localhost:8002", http_client_,
      dummy_token_provider_cache);

  EXPECT_SUCCESS(pbs_client1->Init());
  EXPECT_SUCCESS(pbs_client2->Init());
  EXPECT_SUCCESS(pbs_client3->Init());

  EXPECT_SUCCESS(pbs_client1->Run());
  EXPECT_SUCCESS(pbs_client2->Run());
  EXPECT_SUCCESS(pbs_client3->Run());

  // TEST

  // Wait for the lease to be obtained and partition to be loaded.
  // Ex: 2 * lease duration of 10 seconds.
  sleep_for(seconds(20));

  // Send one BEGIN request to PBS 1.
  auto transaction_id_1 = InitiateTransactionOnPBSClient(pbs_client1);

  // Send one BEGIN request to PBS 2.
  auto transaction_id_2 = InitiateTransactionOnPBSClient(pbs_client2);

  // Send one BEGIN request to PBS 3.
  auto transaction_id_3 = InitiateTransactionOnPBSClient(pbs_client3);

  // Now search for the three transactions using all the clients, this ensures
  // that same partition is not being served on multiple nodes.
  vector<Uuid> transactions = {transaction_id_1, transaction_id_2,
                               transaction_id_3};
  for (const auto& transaction : transactions) {
    GetTransactionStatusOnPBSClient(pbs_client1, transaction);
  }
  for (const auto& transaction : transactions) {
    GetTransactionStatusOnPBSClient(pbs_client2, transaction);
  }
  for (const auto& transaction : transactions) {
    GetTransactionStatusOnPBSClient(pbs_client3, transaction);
  }

  // Stop
  EXPECT_SUCCESS(pbs_client1->Stop());
  EXPECT_SUCCESS(pbs_client2->Stop());
  EXPECT_SUCCESS(pbs_client3->Stop());

  EXPECT_SUCCESS(pbs_instance1.Stop());
  EXPECT_SUCCESS(pbs_instance2.Stop());
  EXPECT_SUCCESS(pbs_instance3.Stop());
}
}  // namespace google::scp::pbs::test
