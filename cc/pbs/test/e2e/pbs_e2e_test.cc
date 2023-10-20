// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/http2_client/src/http2_client.h"
#include "core/interface/authorization_service_interface.h"
#include "core/interface/configuration_keys.h"
#include "core/logger/src/log_providers/syslog/syslog_log_provider.h"
#include "core/logger/src/logger.h"
#include "core/test/utils/conditional_wait.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/pbs_client/src/pbs_transactional_client.h"
#include "pbs/pbs_server/src/pbs_instance/pbs_instance.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::Byte;
using google::scp::core::BytesBuffer;
using google::scp::core::ConfigProviderInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::HttpClient;
using google::scp::core::HttpHeaders;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;
using google::scp::core::LoggerInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::GlobalLogger;
using google::scp::core::common::Uuid;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::logger::Logger;
using google::scp::core::logger::log_providers::SyslogLogProvider;
using google::scp::core::test::ResultIs;
using google::scp::core::test::WaitUntil;
using google::scp::pbs::PBSInstance;
using google::scp::pbs::PrivacyBudgetServiceTransactionalClient;
using std::atomic;
using std::getenv;
using std::make_shared;
using std::make_unique;
using std::runtime_error;
using std::shared_ptr;
using std::stoul;
using std::string;
using std::unique_ptr;
using std::vector;

namespace google::scp::pbs::test::e2e {
bool ReadEnvironmentVariable(string config_prefix, string key,
                             string& output_value) {
  auto environment_variable = config_prefix + key;
  auto* read_value = getenv(environment_variable.c_str());
  if (read_value == NULL) {
    return false;
  }
  output_value = read_value;
  std::cout << "Environment Variable: " << environment_variable << "="
            << output_value << ";" << std::endl;
  return true;
}

bool ReadSizeT(string config_prefix, string key, size_t& size_value) {
  string output_value;
  if (!ReadEnvironmentVariable(config_prefix, key, output_value)) {
    return false;
  }
  size_value = stoul(output_value.c_str(), nullptr, 0);
  return true;
}

bool ReadString(string config_prefix, string key, string& value) {
  string output_value;
  if (!ReadEnvironmentVariable(config_prefix, key, output_value)) {
    return false;
  }
  value = string(output_value);
  return true;
}

struct PBSE2EInstanceConfig {
  // PBS Configurations
  size_t async_executor_queue_size = 100000;
  size_t thread_pool_size = 16;
  size_t io_async_executor_queue_size = 100000;
  size_t io_thread_pool_size = 2000;
  size_t transaction_manager_capacity = 100000;
  size_t http2server_thread_pool_size = 256;
  // The following configurations are intended for the leasable lock's
  // nosql_database_provider, see nosql_database_provider_for_leasable_lock_.
  size_t async_executor_thread_pool_size_for_lease_db_requests = 2;
  size_t async_executor_queue_size_for_lease_db_requests = 10000;

  std::shared_ptr<std::string> journal_bucket_name;
  std::shared_ptr<std::string> journal_partition_name;
  std::shared_ptr<std::string> dynamo_db_table_name;

  std::shared_ptr<std::string> host_address;
  std::shared_ptr<std::string> host_port;
  std::shared_ptr<std::string> health_port;

  std::shared_ptr<std::string> auth_service_endpoint;
  std::shared_ptr<std::string> cloud_service_region;
  std::shared_ptr<std::string> metrics_namespace;
  bool metrics_batch_push_enabled = false;

  // Remote configurations
  std::shared_ptr<std::string> remote_host_address;
  std::shared_ptr<std::string> remote_host_region;
  std::shared_ptr<std::string> remote_auth_service_endpoint;
  std::shared_ptr<std::string> remote_claimed_identity;
  std::shared_ptr<std::string> remote_assume_role_arn;
  std::shared_ptr<std::string> remote_assume_role_external_id;
};

shared_ptr<ConfigProviderInterface> GetConfigProvider(string config_prefix) {
  PBSE2EInstanceConfig pbs_instance_config;
  auto config_provider = make_shared<MockConfigProvider>();
  EXPECT_TRUE(ReadSizeT(config_prefix, "ASYNC_EXECUTOR_QUEUE_SIZE",
                        pbs_instance_config.async_executor_queue_size));
  config_provider->Set(kAsyncExecutorQueueSize,
                       pbs_instance_config.async_executor_queue_size);
  EXPECT_TRUE(ReadSizeT(config_prefix, "ASYNC_EXECUTOR_THREAD_POOL_SIZE",
                        pbs_instance_config.thread_pool_size));
  config_provider->Set(kAsyncExecutorThreadsCount,
                       pbs_instance_config.thread_pool_size);
  EXPECT_TRUE(ReadSizeT(config_prefix, "TRANSACTION_MANAGER_CAPACITY",
                        pbs_instance_config.transaction_manager_capacity));
  config_provider->Set(kTransactionManagerCapacity,
                       pbs_instance_config.transaction_manager_capacity);
  pbs_instance_config.journal_bucket_name = make_shared<string>();
  EXPECT_TRUE(ReadString(config_prefix, "BUCKET_NAME",
                         *pbs_instance_config.journal_bucket_name));
  config_provider->Set(kJournalServiceBucketName,
                       *pbs_instance_config.journal_bucket_name);
  pbs_instance_config.journal_partition_name = make_shared<string>();
  EXPECT_TRUE(ReadString(config_prefix, "PARTITION_NAME",
                         *pbs_instance_config.journal_partition_name));
  config_provider->Set(kJournalServicePartitionName,
                       *pbs_instance_config.journal_partition_name);
  pbs_instance_config.dynamo_db_table_name = make_shared<string>();
  EXPECT_TRUE(ReadString(config_prefix, "BUDGET_KEY_TABLE_NAME",
                         *pbs_instance_config.dynamo_db_table_name));
  config_provider->Set(kBudgetKeyTableName,
                       *pbs_instance_config.dynamo_db_table_name);
  pbs_instance_config.host_address = make_shared<string>();
  EXPECT_TRUE(ReadString(config_prefix, "HOST_ADDRESS",
                         *pbs_instance_config.host_address));
  config_provider->Set(kPrivacyBudgetServiceHostAddress,
                       *pbs_instance_config.host_address);
  pbs_instance_config.host_port = make_shared<string>();
  EXPECT_TRUE(
      ReadString(config_prefix, "HOST_PORT", *pbs_instance_config.host_port));
  config_provider->Set(kPrivacyBudgetServiceHostPort,
                       *pbs_instance_config.host_port);
  pbs_instance_config.host_port = make_shared<string>();
  EXPECT_TRUE(ReadString(config_prefix, "HEALTH_PORT",
                         *pbs_instance_config.health_port));
  config_provider->Set(kPrivacyBudgetServiceHealthPort,
                       *pbs_instance_config.health_port);
  pbs_instance_config.remote_host_address = make_shared<string>();
  EXPECT_TRUE(ReadString(config_prefix, "REMOTE_HOST_ADDRESS",
                         *pbs_instance_config.remote_host_address));
  config_provider->Set(kRemotePrivacyBudgetServiceHostAddress,
                       *pbs_instance_config.remote_host_address);
  pbs_instance_config.auth_service_endpoint = make_shared<string>();
  EXPECT_TRUE(ReadString(config_prefix, "AUTH_SERVICE_ENDPOINT",
                         *pbs_instance_config.auth_service_endpoint));
  config_provider->Set(kAuthServiceEndpoint,
                       *pbs_instance_config.auth_service_endpoint);
  pbs_instance_config.cloud_service_region = make_shared<string>();
  EXPECT_TRUE(ReadString(config_prefix, "CLOUD_SERVICE_REGION",
                         *pbs_instance_config.cloud_service_region));
  config_provider->Set(core::kCloudServiceRegion,
                       *pbs_instance_config.cloud_service_region);

  return config_provider;
}

void StartServer(PBSInstance& pbs_instance) {
  auto execution_result = pbs_instance.Init();
  if (!execution_result.Successful()) {
    std::cout << core::errors::GetErrorMessage(execution_result.status_code);
  }
  EXPECT_SUCCESS(execution_result);
  execution_result = pbs_instance.Run();
  if (!execution_result.Successful()) {
    std::cout << core::errors::GetErrorMessage(execution_result.status_code);
  }
  EXPECT_SUCCESS(execution_result);
}

TEST(PBSE2ETest, TwoServersConsumeBudget) {
  auto pbs_server_1_config_provider = GetConfigProvider("PBS1_");
  auto pbs_server_2_config_provider = GetConfigProvider("PBS2_");
  PBSInstance pbs_instance_1(pbs_server_1_config_provider);
  PBSInstance pbs_instance_2(pbs_server_2_config_provider);

  TestLoggingUtils::EnableLogOutputToSyslog();

  if (fork() == 0) {
    StartServer(pbs_instance_1);
    std::cout << "Started service 1" << std::endl;
    while (true) {}
    return;
  }
  if (fork() == 0) {
    StartServer(pbs_instance_2);
    std::cout << "Started service 2" << std::endl;
    while (true) {}
    return;
  }
  std::this_thread::sleep_for(std::chrono::seconds(15));
  string pbs1_host;
  string pbs1_port;
  pbs_server_1_config_provider->Get(kPrivacyBudgetServiceHostAddress,
                                    pbs1_host);
  pbs_server_1_config_provider->Get(kPrivacyBudgetServiceHostPort, pbs1_port);
  string pbs2_host;
  string pbs2_port;
  pbs_server_2_config_provider->Get(kPrivacyBudgetServiceHostAddress,
                                    pbs2_host);
  pbs_server_2_config_provider->Get(kPrivacyBudgetServiceHostPort, pbs2_port);
  string pbs1_endpoint = string("http://") + pbs1_host + ":" + pbs1_port;
  string pbs2_endpoint = string("http://") + pbs2_host + ":" + pbs2_port;
  string reporting_origin;
  EXPECT_TRUE(ReadString("", "REPORTING_ORIGIN", reporting_origin));
  string pbs1_auth_endpoint;
  pbs_server_1_config_provider->Get(kAuthServiceEndpoint, pbs1_auth_endpoint);
  string pbs1_region;
  pbs_server_1_config_provider->Get(core::kCloudServiceRegion, pbs1_region);
  string pbs2_auth_endpoint;
  pbs_server_2_config_provider->Get(kAuthServiceEndpoint, pbs2_auth_endpoint);
  string pbs2_region;
  pbs_server_2_config_provider->Get(core::kCloudServiceRegion, pbs2_region);
  PrivacyBudgetServiceTransactionalClient pbs_transactional_client(
      reporting_origin, pbs1_region, pbs1_endpoint, pbs1_auth_endpoint,
      pbs2_region, pbs2_endpoint, pbs2_auth_endpoint);
  EXPECT_SUCCESS(pbs_transactional_client.Init());
  EXPECT_SUCCESS(pbs_transactional_client.Run());
  AsyncContext<ConsumeBudgetTransactionRequest,
               ConsumeBudgetTransactionResponse>
      consume_budget_transaction_context;
  consume_budget_transaction_context.request =
      make_shared<ConsumeBudgetTransactionRequest>();
  consume_budget_transaction_context.request->budget_keys =
      make_shared<vector<ConsumeBudgetMetadata>>();
  ConsumeBudgetMetadata metadata;
  metadata.budget_key_name = make_shared<string>("test_budget_key");
  metadata.time_bucket = 12345;
  metadata.token_count = 1;
  consume_budget_transaction_context.request->budget_keys->push_back(metadata);
  consume_budget_transaction_context.request->transaction_id =
      Uuid::GenerateUuid();
  consume_budget_transaction_context.request->transaction_secret =
      make_shared<string>("transaction_secret");
  atomic<bool> finished = false;
  consume_budget_transaction_context.callback =
      [&](AsyncContext<ConsumeBudgetTransactionRequest,
                       ConsumeBudgetTransactionResponse>&
              consume_budget_transaction_context) {
        EXPECT_EQ(consume_budget_transaction_context.result,
                  SuccessExecutionResult());
        finished = true;
      };
  EXPECT_EQ(pbs_transactional_client.ConsumeBudget(
                consume_budget_transaction_context),
            SuccessExecutionResult());
  WaitUntil([&]() { return finished.load(); }, std::chrono::seconds(100000));
  EXPECT_SUCCESS(pbs_transactional_client.Stop());
}
}  // namespace google::scp::pbs::test::e2e
