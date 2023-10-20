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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "core/async_executor/src/async_executor.h"
#include "core/config_provider/mock/mock_config_provider.h"
#include "core/http2_client/mock/mock_http_client.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/configuration_keys.h"
#include "core/interface/token_provider_cache_interface.h"
#include "core/token_provider_cache/mock/token_provider_cache_dummy.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "pbs/interface/configuration_keys.h"
#include "pbs/pbs_server/src/cloud_platform_dependency_factory/gcp/gcp_dependency_factory.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::TokenProviderCacheInterface;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::http2_client::mock::MockHttpClient;
using google::scp::core::test::ResultIs;
using google::scp::core::token_provider_cache::mock::DummyTokenProviderCache;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using google::scp::pbs::GcpDependencyFactory;
using std::make_shared;
using std::shared_ptr;

namespace google::scp::pbs::test {

static constexpr char kTokenServerPath[] =
    "http://metadata/computeMetadata/v1/instance/service-accounts/default/"
    "identity";
constexpr char kBase64EncodedAuthToken[] =
    "someheader."
    "eyJleHAiOjE2NzI3NjA3MDEsImlzcyI6Imlzc3VlciIsImF1ZCI6ImF1ZGllbmNlIiwic3ViIj"
    "oic3ViamVjdCIsImlhdCI6MTY3Mjc1NzEwMX0=.signature";

class GcpCloudDependencyFactoryTest : public ::testing::Test {
 protected:
  GcpCloudDependencyFactoryTest()
      : async_executor1_(make_shared<core::AsyncExecutor>(2, 10000, true)),
        async_executor2_(make_shared<core::AsyncExecutor>(2, 10000, true)),
        mock_http_client_(make_shared<MockHttpClient>()),
        mock_config_provider_(make_shared<MockConfigProvider>()),
        mock_token_provider_cache_(make_shared<DummyTokenProviderCache>()),
        mock_instance_client_provider_(
            make_shared<MockInstanceClientProvider>()),
        gcp_factory_(mock_config_provider_) {}

  void SetUp() override {
    EXPECT_THAT(async_executor1_->Init(), ResultIs(SuccessExecutionResult()));
    EXPECT_THAT(async_executor2_->Init(), ResultIs(SuccessExecutionResult()));

    EXPECT_THAT(async_executor1_->Run(), ResultIs(SuccessExecutionResult()));
    EXPECT_THAT(async_executor2_->Run(), ResultIs(SuccessExecutionResult()));

    // Set up configurations.
    mock_config_provider_->Set(kRemotePrivacyBudgetServiceAssumeRoleArn, "arn");
    mock_config_provider_->Set(kRemotePrivacyBudgetServiceAssumeRoleExternalId,
                               "1234");
    mock_config_provider_->Set(core::kCloudServiceRegion, "us-east-1");
    mock_config_provider_->Set(kAuthServiceEndpoint, "https://www.auth.com");
    mock_config_provider_->Set(kServiceMetricsNamespace, "metric");
    mock_config_provider_->Set(kRemotePrivacyBudgetServiceCloudServiceRegion,
                               "us-east-1");
    mock_config_provider_->Set(kRemotePrivacyBudgetServiceAuthServiceEndpoint,
                               "https://www.authremote.com");
    mock_config_provider_->Set(kRemotePrivacyBudgetServiceClaimedIdentity,
                               "identity.com");
    mock_config_provider_->Set(kRemotePrivacyBudgetServiceHostAddress,
                               "https://www.pbs.com");
    mock_config_provider_->SetBool(kServiceMetricsBatchPush, false);
    mock_config_provider_->Set(kServiceMetricsBatchTimeDurationMs, "3000");
    mock_config_provider_->Set(kBudgetKeyTableName, "budget_key");
    mock_config_provider_->Set(kPBSPartitionLockTableNameConfigName,
                               "partition_lock_table");
    mock_config_provider_->Set(core::kGcpProjectId, "project");
    mock_config_provider_->Set(core::kSpannerInstance, "kinstance");
    mock_config_provider_->Set(core::kSpannerDatabase, "database");

    EXPECT_THAT(gcp_factory_.Init(), ResultIs(SuccessExecutionResult()));
  }

  void TearDown() override {
    EXPECT_THAT(async_executor1_->Stop(), ResultIs(SuccessExecutionResult()));
    EXPECT_THAT(async_executor2_->Stop(), ResultIs(SuccessExecutionResult()));
  }

  shared_ptr<AsyncExecutorInterface> async_executor1_;
  shared_ptr<AsyncExecutorInterface> async_executor2_;
  shared_ptr<MockHttpClient> mock_http_client_;
  shared_ptr<MockConfigProvider> mock_config_provider_;
  shared_ptr<TokenProviderCacheInterface> mock_token_provider_cache_;
  shared_ptr<InstanceClientProviderInterface> mock_instance_client_provider_;
  GcpDependencyFactory gcp_factory_;
};

TEST_F(GcpCloudDependencyFactoryTest,
       ConstructAuthorizationTokenProviderCache) {
  mock_http_client_->request_mock.path =
      make_shared<core::Uri>(kTokenServerPath);
  mock_http_client_->response_mock.body =
      core::BytesBuffer(kBase64EncodedAuthToken);
  auto cache = gcp_factory_.ConstructAuthorizationTokenProviderCache(
      async_executor1_, async_executor2_, mock_http_client_);
  EXPECT_THAT(cache->Init(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(cache->Run(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(cache->Stop(), ResultIs(SuccessExecutionResult()));
}

TEST_F(GcpCloudDependencyFactoryTest, ConstructAuthorizationProxyClient) {
  auto proxy = gcp_factory_.ConstructAuthorizationProxyClient(
      async_executor1_, mock_http_client_);
  EXPECT_THAT(proxy->Init(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(proxy->Run(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(proxy->Stop(), ResultIs(SuccessExecutionResult()));
}

TEST_F(GcpCloudDependencyFactoryTest, ConstructBlobStorageClient) {
  auto blob_client = gcp_factory_.ConstructBlobStorageClient(async_executor1_,
                                                             async_executor2_);
  EXPECT_THAT(blob_client->Init(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(blob_client->Run(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(blob_client->Stop(), ResultIs(SuccessExecutionResult()));
}

TEST_F(GcpCloudDependencyFactoryTest, ConstructNoSQLDatabaseClient) {
  auto no_sql_client = gcp_factory_.ConstructNoSQLDatabaseClient(
      async_executor1_, async_executor2_);
  EXPECT_THAT(no_sql_client->Init(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(no_sql_client->Run(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(no_sql_client->Stop(), ResultIs(SuccessExecutionResult()));
}

TEST_F(GcpCloudDependencyFactoryTest, ConstructInstanceMetadataClient) {
  auto metadata_client = gcp_factory_.ConstructInstanceMetadataClient(
      mock_http_client_, nullptr, /*http2_client*/ nullptr,
      /*async_executor*/ nullptr,
      /*io_async_executor*/ nullptr /*auth_token_provider*/);
  EXPECT_THAT(metadata_client->Init(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(metadata_client->Run(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(metadata_client->Stop(), ResultIs(SuccessExecutionResult()));
}

TEST_F(GcpCloudDependencyFactoryTest, ConstructMetricClient) {
  auto metric_client = gcp_factory_.ConstructMetricClient(
      async_executor1_, async_executor2_, mock_instance_client_provider_);
  EXPECT_THAT(metric_client->Init(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(metric_client->Run(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(metric_client->Stop(), ResultIs(SuccessExecutionResult()));
}

TEST_F(GcpCloudDependencyFactoryTest, ConstructRemoteCoordinatorPBSClient) {
  auto client = gcp_factory_.ConstructRemoteCoordinatorPBSClient(
      mock_http_client_, mock_token_provider_cache_);
  EXPECT_THAT(client->Init(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(client->Run(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(client->Stop(), ResultIs(SuccessExecutionResult()));
}

}  // namespace google::scp::pbs::test
