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

#include "cc/core/async_executor/src/async_executor.h"
#include "cc/core/config_provider/mock/mock_config_provider.h"
#include "cc/core/http2_client/mock/mock_http_client.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/configuration_keys.h"
#include "cc/pbs/interface/configuration_keys.h"
#include "cc/pbs/pbs_server/src/cloud_platform_dependency_factory/gcp/gcp_dependency_factory.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/core/test/interface/execution_result_matchers.h"

namespace privacy_sandbox::pbs {
namespace {
using ::privacy_sandbox::pbs_common::AsyncExecutor;
using ::privacy_sandbox::pbs_common::AsyncExecutorInterface;
using ::privacy_sandbox::pbs_common::ExecutionResult;
using ::privacy_sandbox::pbs_common::kCloudServiceRegion;
using ::privacy_sandbox::pbs_common::kGcpProjectId;
using ::privacy_sandbox::pbs_common::kSpannerDatabase;
using ::privacy_sandbox::pbs_common::kSpannerInstance;
using ::privacy_sandbox::pbs_common::MockConfigProvider;
using ::privacy_sandbox::pbs_common::MockHttpClient;
using ::privacy_sandbox::pbs_common::ResultIs;
using ::privacy_sandbox::pbs_common::SuccessExecutionResult;
using std::make_shared;
using std::shared_ptr;

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
      : async_executor1_(make_shared<AsyncExecutor>(2, 10000, true)),
        async_executor2_(make_shared<AsyncExecutor>(2, 10000, true)),
        mock_http_client_(make_shared<MockHttpClient>()),
        mock_config_provider_(make_shared<MockConfigProvider>()),
        gcp_factory_(mock_config_provider_) {}

  void SetUp() override {
    EXPECT_THAT(async_executor1_->Init(), ResultIs(SuccessExecutionResult()));
    EXPECT_THAT(async_executor2_->Init(), ResultIs(SuccessExecutionResult()));

    EXPECT_THAT(async_executor1_->Run(), ResultIs(SuccessExecutionResult()));
    EXPECT_THAT(async_executor2_->Run(), ResultIs(SuccessExecutionResult()));

    // Set up configurations.
    mock_config_provider_->Set(kCloudServiceRegion, "us-east-1");
    mock_config_provider_->Set(kAuthServiceEndpoint, "https://www.auth.com");
    mock_config_provider_->Set(kBudgetKeyTableName, "budget_key");
    mock_config_provider_->Set(kGcpProjectId, "project");
    mock_config_provider_->Set(kSpannerInstance, "kinstance");
    mock_config_provider_->Set(kSpannerDatabase, "database");

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
  GcpDependencyFactory gcp_factory_;
};

TEST_F(GcpCloudDependencyFactoryTest, ConstructAuthorizationProxyClient) {
  auto proxy = gcp_factory_.ConstructAuthorizationProxyClient(
      async_executor1_, mock_http_client_);
  EXPECT_THAT(proxy->Init(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(proxy->Run(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(proxy->Stop(), ResultIs(SuccessExecutionResult()));
}

TEST_F(GcpCloudDependencyFactoryTest, ConstructBudgetConsumptionHelper) {
  auto helper = gcp_factory_.ConstructBudgetConsumptionHelper(
      async_executor1_.get(), async_executor2_.get());
  ASSERT_NE(helper, nullptr);
  EXPECT_THAT(helper->Init(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(helper->Run(), ResultIs(SuccessExecutionResult()));
  EXPECT_THAT(helper->Stop(), ResultIs(SuccessExecutionResult()));
}
}  // namespace
}  // namespace privacy_sandbox::pbs
