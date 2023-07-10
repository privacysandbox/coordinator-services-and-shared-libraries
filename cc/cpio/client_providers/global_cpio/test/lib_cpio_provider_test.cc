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

#include <aws/core/Aws.h>
#include <gmock/gmock.h>

#include "core/async_executor/src/async_executor.h"
#include "core/interface/async_executor_interface.h"
#include "cpio/client_providers/global_cpio/mock/mock_lib_cpio_provider_with_overrides.h"
#include "public/core/interface/execution_result.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::HttpClientInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::client_providers::AuthTokenProviderInterface;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::RoleCredentialsProviderInterface;
using google::scp::cpio::client_providers::mock::
    MockLibCpioProviderWithOverrides;
using std::make_unique;
using std::shared_ptr;
using ::testing::IsNull;
using ::testing::NotNull;

namespace google::scp::cpio::test {
class LibCpioProviderTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    SDKOptions options;
    InitAPI(options);
  }

  static void TearDownTestSuite() {
    SDKOptions options;
    ShutdownAPI(options);
  }
};

TEST_F(LibCpioProviderTest, InstanceClientProviderNotCreatedInInt) {
  auto lib_cpio_provider = make_unique<MockLibCpioProviderWithOverrides>();
  EXPECT_EQ(lib_cpio_provider->Init(), SuccessExecutionResult());
  EXPECT_EQ(lib_cpio_provider->Run(), SuccessExecutionResult());
  EXPECT_THAT(lib_cpio_provider->GetInstanceClientProviderMember(), IsNull());

  shared_ptr<InstanceClientProviderInterface> instance_client_provider;
  EXPECT_EQ(
      lib_cpio_provider->GetInstanceClientProvider(instance_client_provider),
      SuccessExecutionResult());
  EXPECT_THAT(instance_client_provider, NotNull());
  EXPECT_EQ(lib_cpio_provider->Stop(), SuccessExecutionResult());
}

TEST_F(LibCpioProviderTest, AsyncExecutorNotCreatedInInit) {
  auto lib_cpio_provider = make_unique<MockLibCpioProviderWithOverrides>();
  EXPECT_EQ(lib_cpio_provider->Init(), SuccessExecutionResult());
  EXPECT_EQ(lib_cpio_provider->Run(), SuccessExecutionResult());
  EXPECT_THAT(lib_cpio_provider->GetAsyncExecutorMember(), IsNull());

  shared_ptr<AsyncExecutorInterface> async_executor;
  EXPECT_EQ(lib_cpio_provider->GetAsyncExecutor(async_executor),
            SuccessExecutionResult());
  EXPECT_THAT(async_executor, NotNull());

  EXPECT_EQ(lib_cpio_provider->Stop(), SuccessExecutionResult());
}

TEST_F(LibCpioProviderTest, IOAsyncExecutorNotCreatedInInit) {
  auto lib_cpio_provider = make_unique<MockLibCpioProviderWithOverrides>();
  EXPECT_EQ(lib_cpio_provider->Init(), SuccessExecutionResult());
  EXPECT_EQ(lib_cpio_provider->Run(), SuccessExecutionResult());
  EXPECT_THAT(lib_cpio_provider->GetIOAsyncExecutorMember(), IsNull());

  shared_ptr<AsyncExecutorInterface> io_async_executor;
  EXPECT_EQ(lib_cpio_provider->GetIOAsyncExecutor(io_async_executor),
            SuccessExecutionResult());
  EXPECT_THAT(io_async_executor, NotNull());

  EXPECT_EQ(lib_cpio_provider->Stop(), SuccessExecutionResult());
}

TEST_F(LibCpioProviderTest, Http2ClientNotCreatedInInit) {
  auto lib_cpio_provider = make_unique<MockLibCpioProviderWithOverrides>();
  EXPECT_EQ(lib_cpio_provider->Init(), SuccessExecutionResult());
  EXPECT_EQ(lib_cpio_provider->Run(), SuccessExecutionResult());
  EXPECT_THAT(lib_cpio_provider->GetHttp2ClientMember(), IsNull());

  shared_ptr<HttpClientInterface> http2_client;
  EXPECT_EQ(lib_cpio_provider->GetHttpClient(http2_client),
            SuccessExecutionResult());
  EXPECT_THAT(http2_client, NotNull());

  EXPECT_EQ(lib_cpio_provider->Stop(), SuccessExecutionResult());
}

TEST_F(LibCpioProviderTest, Http1ClientNotCreatedInInit) {
  auto lib_cpio_provider = make_unique<MockLibCpioProviderWithOverrides>();
  EXPECT_EQ(lib_cpio_provider->Init(), SuccessExecutionResult());
  EXPECT_EQ(lib_cpio_provider->Run(), SuccessExecutionResult());
  EXPECT_THAT(lib_cpio_provider->GetHttp1ClientMember(), IsNull());

  shared_ptr<HttpClientInterface> http1_client;
  EXPECT_EQ(lib_cpio_provider->GetHttp1Client(http1_client),
            SuccessExecutionResult());
  EXPECT_THAT(http1_client, NotNull());

  EXPECT_EQ(lib_cpio_provider->Stop(), SuccessExecutionResult());
}

TEST_F(LibCpioProviderTest, RoleCredentialsProviderNotCreatedInInit) {
  auto lib_cpio_provider = make_unique<MockLibCpioProviderWithOverrides>();
  EXPECT_EQ(lib_cpio_provider->Init(), SuccessExecutionResult());
  EXPECT_EQ(lib_cpio_provider->Run(), SuccessExecutionResult());
  EXPECT_THAT(lib_cpio_provider->GetRoleCredentialsProviderMember(), IsNull());

  shared_ptr<RoleCredentialsProviderInterface> role_credentials_provider;
  EXPECT_EQ(
      lib_cpio_provider->GetRoleCredentialsProvider(role_credentials_provider),
      SuccessExecutionResult());
  EXPECT_THAT(role_credentials_provider, NotNull());

  EXPECT_EQ(lib_cpio_provider->Stop(), SuccessExecutionResult());
}

TEST_F(LibCpioProviderTest, AuthTokenProviderNotCreatedInInit) {
  auto lib_cpio_provider = make_unique<MockLibCpioProviderWithOverrides>();
  EXPECT_EQ(lib_cpio_provider->Init(), SuccessExecutionResult());
  EXPECT_EQ(lib_cpio_provider->Run(), SuccessExecutionResult());
  EXPECT_THAT(lib_cpio_provider->GetAuthTokenProviderMember(), IsNull());

  shared_ptr<AuthTokenProviderInterface> auth_token_provider;
  EXPECT_EQ(lib_cpio_provider->GetAuthTokenProvider(auth_token_provider),
            SuccessExecutionResult());
  EXPECT_THAT(auth_token_provider, NotNull());

  EXPECT_EQ(lib_cpio_provider->Stop(), SuccessExecutionResult());
}
}  // namespace google::scp::cpio::test
