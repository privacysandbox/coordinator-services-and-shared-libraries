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

#include "pbs/pbs_server/src/pbs_instance/pbs_instance_configuration.h"

#include <gtest/gtest.h>

#include <string>

#include "core/config_provider/mock/mock_config_provider.h"
#include "core/config_provider/src/env_config_provider.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/configuration_keys.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/pbs_server/src/pbs_instance/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::ConfigProviderInterface;
using google::scp::core::EnvConfigProvider;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::errors::SC_PBS_INVALID_HTTP2_SERVER_CERT_FILE_PATH;
using google::scp::core::errors::
    SC_PBS_INVALID_HTTP2_SERVER_PRIVATE_KEY_FILE_PATH;
using google::scp::core::test::ResultIs;
using std::make_shared;
using std::shared_ptr;

namespace google::scp::pbs::test {

static void SetAllConfigs() {
  setenv(kAsyncExecutorQueueSize, "10000", 1);
  setenv(kAsyncExecutorThreadsCount, "10", 1);
  setenv(kIOAsyncExecutorQueueSize, "10000", 1);
  setenv(kIOAsyncExecutorThreadsCount, "10", 1);
  setenv(kTransactionManagerCapacity, "10000", 1);
  setenv(kJournalServiceBucketName, "bucket", 1);
  setenv(kJournalServicePartitionName, "00000000-0000-0000-0000-000000000000",
         1);
  setenv(kPrivacyBudgetServiceHostAddress, "0.0.0.0", 1);
  setenv(kPrivacyBudgetServiceHostPort, "8000", 1);
  setenv(kPrivacyBudgetServiceHealthPort, "8001", 1);
  setenv(kRemotePrivacyBudgetServiceCloudServiceRegion, "region", 1);
  setenv(kRemotePrivacyBudgetServiceAuthServiceEndpoint, "https://auth.com", 1);
  setenv(kRemotePrivacyBudgetServiceClaimedIdentity, "remote-id", 1);
  setenv(kRemotePrivacyBudgetServiceAssumeRoleArn, "arn", 1);
  setenv(kRemotePrivacyBudgetServiceAssumeRoleExternalId, "id", 1);
  setenv(kRemotePrivacyBudgetServiceHostAddress, "https://remote.com", 1);
  setenv(kAuthServiceEndpoint, "https://auth.com", 1);
  setenv(core::kCloudServiceRegion, "region", 1);
  setenv(kServiceMetricsNamespace, "ns", 1);
  setenv(kTotalHttp2ServerThreadsCount, "10", 1);
  setenv(kPBSPartitionLockTableNameConfigName, "partition_lock_table", 1);
  // NOTE: Any sets here should accompany the Unsets at UnsetAllConfigs.
}

static void UnsetAllConfigs() {
  unsetenv(kAsyncExecutorQueueSize);
  unsetenv(kAsyncExecutorThreadsCount);
  unsetenv(kIOAsyncExecutorQueueSize);
  unsetenv(kIOAsyncExecutorThreadsCount);
  unsetenv(kTransactionManagerCapacity);
  unsetenv(kJournalServiceBucketName);
  unsetenv(kJournalServicePartitionName);
  unsetenv(kPrivacyBudgetServiceHostAddress);
  unsetenv(kPrivacyBudgetServiceHostPort);
  unsetenv(kPrivacyBudgetServiceHealthPort);
  unsetenv(kRemotePrivacyBudgetServiceCloudServiceRegion);
  unsetenv(kRemotePrivacyBudgetServiceAuthServiceEndpoint);
  unsetenv(kRemotePrivacyBudgetServiceClaimedIdentity);
  unsetenv(kRemotePrivacyBudgetServiceAssumeRoleArn);
  unsetenv(kRemotePrivacyBudgetServiceAssumeRoleExternalId);
  unsetenv(kRemotePrivacyBudgetServiceHostAddress);
  unsetenv(kAuthServiceEndpoint);
  unsetenv(core::kCloudServiceRegion);
  unsetenv(kServiceMetricsNamespace);
  unsetenv(kTotalHttp2ServerThreadsCount);
  unsetenv(kPBSPartitionLockTableNameConfigName);
  unsetenv(kHttp2ServerUseTls);
  unsetenv(kHttp2ServerPrivateKeyFilePath);
  unsetenv(kHttp2ServerCertificateFilePath);
}

class PBSInstanceConfiguration : public ::testing::Test {
 protected:
  PBSInstanceConfiguration() {
    // Reset Env.
    UnsetAllConfigs();
    SetAllConfigs();

    env_config_provider_ = make_shared<EnvConfigProvider>();
  }

  ~PBSInstanceConfiguration() { UnsetAllConfigs(); }

  shared_ptr<ConfigProviderInterface> env_config_provider_;
};

TEST_F(PBSInstanceConfiguration,
       ReadConfigurationShouldFailIfUseTlsButNoPrivateKeyPath) {
  setenv(kHttp2ServerUseTls, "true", 1);
  setenv(kHttp2ServerCertificateFilePath, "/cert/path", 1);
  // Error if unset
  auto pbs_instance_config_or =
      GetPBSInstanceConfigFromConfigProvider(env_config_provider_);
  EXPECT_THAT(pbs_instance_config_or,
              ResultIs(FailureExecutionResult(
                  SC_PBS_INVALID_HTTP2_SERVER_PRIVATE_KEY_FILE_PATH)));
  // Error if empty
  setenv(kHttp2ServerPrivateKeyFilePath, "", 1);
  pbs_instance_config_or =
      GetPBSInstanceConfigFromConfigProvider(env_config_provider_);
  EXPECT_THAT(pbs_instance_config_or,
              ResultIs(FailureExecutionResult(
                  SC_PBS_INVALID_HTTP2_SERVER_PRIVATE_KEY_FILE_PATH)));
}

TEST_F(PBSInstanceConfiguration,
       ReadConfigurationShouldFailIfUseTlsButNoCertificatePath) {
  setenv(kHttp2ServerUseTls, "true", 1);
  setenv(kHttp2ServerPrivateKeyFilePath, "/key/path", 1);
  // Error if unset
  auto pbs_instance_config_or =
      GetPBSInstanceConfigFromConfigProvider(env_config_provider_);
  EXPECT_THAT(pbs_instance_config_or,
              ResultIs(FailureExecutionResult(
                  SC_PBS_INVALID_HTTP2_SERVER_CERT_FILE_PATH)));

  setenv(kHttp2ServerCertificateFilePath, "", 1);
  // Error if empty
  pbs_instance_config_or =
      GetPBSInstanceConfigFromConfigProvider(env_config_provider_);
  EXPECT_THAT(pbs_instance_config_or,
              ResultIs(FailureExecutionResult(
                  SC_PBS_INVALID_HTTP2_SERVER_CERT_FILE_PATH)));
}

TEST_F(PBSInstanceConfiguration,
       ReadConfigurationShouldSucceedIfUseTlsAndCertAndKeyPathsAreSet) {
  setenv(kHttp2ServerUseTls, "true", 1);
  setenv(kHttp2ServerPrivateKeyFilePath, "/key/path", 1);
  setenv(kHttp2ServerCertificateFilePath, "/cert/path", 1);

  auto pbs_instance_config_or =
      GetPBSInstanceConfigFromConfigProvider(env_config_provider_);
  EXPECT_SUCCESS(pbs_instance_config_or);

  EXPECT_TRUE(pbs_instance_config_or->http2_server_use_tls);
  EXPECT_EQ(*pbs_instance_config_or->http2_server_private_key_file_path,
            "/key/path");
  EXPECT_EQ(*pbs_instance_config_or->http2_server_certificate_file_path,
            "/cert/path");
}

TEST_F(PBSInstanceConfiguration,
       ReadConfigurationShouldSucceedIfMissingUseTlsOrEmpty) {
  // Missing use tls config
  auto pbs_instance_config_or =
      GetPBSInstanceConfigFromConfigProvider(env_config_provider_);
  EXPECT_SUCCESS(pbs_instance_config_or);

  EXPECT_FALSE(pbs_instance_config_or->http2_server_use_tls);
  EXPECT_EQ(*pbs_instance_config_or->http2_server_private_key_file_path, "");
  EXPECT_EQ(*pbs_instance_config_or->http2_server_certificate_file_path, "");

  // Empty use tls config
  setenv(kHttp2ServerUseTls, "", 1);
  pbs_instance_config_or =
      GetPBSInstanceConfigFromConfigProvider(env_config_provider_);
  EXPECT_SUCCESS(pbs_instance_config_or);

  EXPECT_FALSE(pbs_instance_config_or->http2_server_use_tls);
  EXPECT_EQ(*pbs_instance_config_or->http2_server_private_key_file_path, "");
  EXPECT_EQ(*pbs_instance_config_or->http2_server_certificate_file_path, "");
}

TEST_F(PBSInstanceConfiguration,
       ReadConfigurationShouldSucceedIfUseTlsParsingFails) {
  // Does not parse to bool
  setenv(kHttp2ServerUseTls, "t", 1);

  auto pbs_instance_config_or =
      GetPBSInstanceConfigFromConfigProvider(env_config_provider_);
  EXPECT_SUCCESS(pbs_instance_config_or);

  EXPECT_FALSE(pbs_instance_config_or->http2_server_use_tls);
  EXPECT_EQ(*pbs_instance_config_or->http2_server_private_key_file_path, "");
  EXPECT_EQ(*pbs_instance_config_or->http2_server_certificate_file_path, "");

  // Does not parse to bool
  setenv(kHttp2ServerUseTls, "123", 1);

  pbs_instance_config_or =
      GetPBSInstanceConfigFromConfigProvider(env_config_provider_);
  EXPECT_SUCCESS(pbs_instance_config_or);

  EXPECT_FALSE(pbs_instance_config_or->http2_server_use_tls);
  EXPECT_EQ(*pbs_instance_config_or->http2_server_private_key_file_path, "");
  EXPECT_EQ(*pbs_instance_config_or->http2_server_certificate_file_path, "");
}

TEST_F(PBSInstanceConfiguration,
       ReadConfigurationShouldSucceedIfUseTlsIsSetToFalse) {
  setenv(kHttp2ServerUseTls, "false", 1);

  auto pbs_instance_config_or =
      GetPBSInstanceConfigFromConfigProvider(env_config_provider_);
  EXPECT_SUCCESS(pbs_instance_config_or);

  EXPECT_FALSE(pbs_instance_config_or->http2_server_use_tls);
  EXPECT_EQ(*pbs_instance_config_or->http2_server_private_key_file_path, "");
  EXPECT_EQ(*pbs_instance_config_or->http2_server_certificate_file_path, "");
}

TEST_F(PBSInstanceConfiguration, ReadConfigurationReadsAllConfigs) {
  auto config_provider = std::make_shared<MockConfigProvider>();
  config_provider->SetInt(kAsyncExecutorQueueSize, 1);
  config_provider->SetInt(kAsyncExecutorThreadsCount, 2);
  config_provider->SetInt(kIOAsyncExecutorQueueSize, 3);
  config_provider->SetInt(kIOAsyncExecutorThreadsCount, 4);
  config_provider->SetInt(kTransactionManagerCapacity, 5);
  config_provider->Set(kJournalServiceBucketName, "bucket");
  config_provider->Set(kJournalServicePartitionName,
                       "00000000-0000-0000-0000-000000000000");
  config_provider->Set(kPrivacyBudgetServiceHostAddress, "0.0.0.0");
  config_provider->Set(kPrivacyBudgetServiceHostPort, "8000");
  config_provider->Set(kPrivacyBudgetServiceHealthPort, "8001");
  config_provider->Set(kPrivacyBudgetServiceExternalExposedHostPort, "80");
  config_provider->SetInt(kTotalHttp2ServerThreadsCount, 10);
  config_provider->SetBool(kHttp2ServerUseTls, true);
  config_provider->Set(kHttp2ServerPrivateKeyFilePath, "/key/path");
  config_provider->Set(kHttp2ServerCertificateFilePath, "/cert/path");
  config_provider->Set(kPBSPartitionLockTableNameConfigName, "partition_lock");

  auto pbs_config_or = GetPBSInstanceConfigFromConfigProvider(config_provider);
  EXPECT_SUCCESS(pbs_config_or);

  EXPECT_EQ(pbs_config_or->async_executor_queue_size, 1);
  EXPECT_EQ(pbs_config_or->async_executor_thread_pool_size, 2);
  EXPECT_EQ(pbs_config_or->io_async_executor_queue_size, 3);
  EXPECT_EQ(pbs_config_or->io_async_executor_thread_pool_size, 4);
  EXPECT_EQ(pbs_config_or->transaction_manager_capacity, 5);
  EXPECT_EQ(pbs_config_or->http2server_thread_pool_size, 10);
  EXPECT_EQ(pbs_config_or->http2server_thread_pool_size, 10);
  EXPECT_EQ(*pbs_config_or->journal_bucket_name, "bucket");
  EXPECT_EQ(*pbs_config_or->journal_partition_name,
            "00000000-0000-0000-0000-000000000000");
  EXPECT_EQ(*pbs_config_or->host_address, "0.0.0.0");
  EXPECT_EQ(*pbs_config_or->host_port, "8000");
  EXPECT_EQ(*pbs_config_or->external_exposed_host_port, "80");
  EXPECT_EQ(*pbs_config_or->health_port, "8001");
  EXPECT_EQ(*pbs_config_or->http2_server_private_key_file_path, "/key/path");
  EXPECT_EQ(*pbs_config_or->http2_server_certificate_file_path, "/cert/path");
  EXPECT_EQ(*pbs_config_or->partition_lease_table_name, "partition_lock");
  EXPECT_EQ(pbs_config_or->http2_server_use_tls, true);
}
}  // namespace google::scp::pbs::test
