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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/async_executor/src/async_executor.h"
#include "core/interface/async_executor_interface.h"
#include "cpio/client_providers/global_cpio/mock/mock_lib_cpio_provider_with_overrides.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::HttpClientInterface;
using google::scp::cpio::client_providers::AuthTokenProviderInterface;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::RoleCredentialsProviderInterface;
using google::scp::cpio::client_providers::mock::
    MockLibCpioProviderWithOverrides;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using ::testing::IsNull;
using ::testing::NotNull;

namespace google::scp::cpio::test {
TEST(LibCpioProviderTest, InstanceClientProviderNotCreatedInInt) {
  auto lib_cpio_provider = make_unique<MockLibCpioProviderWithOverrides>();
  EXPECT_SUCCESS(lib_cpio_provider->Init());
  EXPECT_SUCCESS(lib_cpio_provider->Run());
  EXPECT_THAT(lib_cpio_provider->GetInstanceClientProviderMember(), IsNull());

  shared_ptr<InstanceClientProviderInterface> instance_client_provider;
  EXPECT_SUCCESS(
      lib_cpio_provider->GetInstanceClientProvider(instance_client_provider));
  EXPECT_THAT(instance_client_provider, NotNull());
  EXPECT_SUCCESS(lib_cpio_provider->Stop());
}

TEST(LibCpioProviderTest, AsyncExecutorNotCreatedInInit) {
  auto lib_cpio_provider = make_unique<MockLibCpioProviderWithOverrides>();
  EXPECT_SUCCESS(lib_cpio_provider->Init());
  EXPECT_SUCCESS(lib_cpio_provider->Run());
  EXPECT_THAT(lib_cpio_provider->GetCpuAsyncExecutorMember(), IsNull());

  shared_ptr<AsyncExecutorInterface> async_executor;
  EXPECT_SUCCESS(lib_cpio_provider->GetCpuAsyncExecutor(async_executor));
  EXPECT_THAT(async_executor, NotNull());

  EXPECT_SUCCESS(lib_cpio_provider->Stop());
}

TEST(LibCpioProviderTest, IOAsyncExecutorNotCreatedInInit) {
  auto lib_cpio_provider = make_unique<MockLibCpioProviderWithOverrides>();
  EXPECT_SUCCESS(lib_cpio_provider->Init());
  EXPECT_SUCCESS(lib_cpio_provider->Run());
  EXPECT_THAT(lib_cpio_provider->GetIoAsyncExecutorMember(), IsNull());

  shared_ptr<AsyncExecutorInterface> io_async_executor;
  EXPECT_SUCCESS(lib_cpio_provider->GetIoAsyncExecutor(io_async_executor));
  EXPECT_THAT(io_async_executor, NotNull());

  EXPECT_SUCCESS(lib_cpio_provider->Stop());
}

TEST(LibCpioProviderTest, Http2ClientNotCreatedInInit) {
  auto lib_cpio_provider = make_unique<MockLibCpioProviderWithOverrides>();
  EXPECT_SUCCESS(lib_cpio_provider->Init());
  EXPECT_SUCCESS(lib_cpio_provider->Run());
  EXPECT_THAT(lib_cpio_provider->GetHttp2ClientMember(), IsNull());

  shared_ptr<HttpClientInterface> http2_client;
  EXPECT_SUCCESS(lib_cpio_provider->GetHttpClient(http2_client));
  EXPECT_THAT(http2_client, NotNull());

  EXPECT_SUCCESS(lib_cpio_provider->Stop());
}

TEST(LibCpioProviderTest, Http1ClientNotCreatedInInit) {
  auto lib_cpio_provider = make_unique<MockLibCpioProviderWithOverrides>();
  EXPECT_SUCCESS(lib_cpio_provider->Init());
  EXPECT_SUCCESS(lib_cpio_provider->Run());
  EXPECT_THAT(lib_cpio_provider->GetHttp1ClientMember(), IsNull());

  shared_ptr<HttpClientInterface> http1_client;
  EXPECT_SUCCESS(lib_cpio_provider->GetHttp1Client(http1_client));
  EXPECT_THAT(http1_client, NotNull());

  EXPECT_SUCCESS(lib_cpio_provider->Stop());
}

TEST(LibCpioProviderTest, RoleCredentialsProviderNotCreatedInInit) {
  auto lib_cpio_provider = make_unique<MockLibCpioProviderWithOverrides>();
  EXPECT_SUCCESS(lib_cpio_provider->Init());
  EXPECT_SUCCESS(lib_cpio_provider->Run());
  EXPECT_THAT(lib_cpio_provider->GetRoleCredentialsProviderMember(), IsNull());

  shared_ptr<RoleCredentialsProviderInterface> role_credentials_provider;
  EXPECT_SUCCESS(
      lib_cpio_provider->GetRoleCredentialsProvider(role_credentials_provider));
  EXPECT_THAT(role_credentials_provider, NotNull());

  EXPECT_SUCCESS(lib_cpio_provider->Stop());
}

TEST(LibCpioProviderTest, AuthTokenProviderNotCreatedInInit) {
  auto lib_cpio_provider = make_unique<MockLibCpioProviderWithOverrides>();
  EXPECT_SUCCESS(lib_cpio_provider->Init());
  EXPECT_SUCCESS(lib_cpio_provider->Run());
  EXPECT_THAT(lib_cpio_provider->GetAuthTokenProviderMember(), IsNull());

  shared_ptr<AuthTokenProviderInterface> auth_token_provider;
  EXPECT_SUCCESS(lib_cpio_provider->GetAuthTokenProvider(auth_token_provider));
  EXPECT_THAT(auth_token_provider, NotNull());

  EXPECT_SUCCESS(lib_cpio_provider->Stop());
}

TEST(LibCpioProviderTest, SetCpuAsyncExecutor) {
  auto lib_cpio_provider = make_unique<MockLibCpioProviderWithOverrides>();
  EXPECT_SUCCESS(lib_cpio_provider->Init());
  EXPECT_SUCCESS(lib_cpio_provider->Run());
  EXPECT_THAT(lib_cpio_provider->GetCpuAsyncExecutorMember(), IsNull());

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<AsyncExecutor>(1, 2);
  EXPECT_SUCCESS(async_executor->Init());
  EXPECT_SUCCESS(async_executor->Run());
  EXPECT_SUCCESS(lib_cpio_provider->SetCpuAsyncExecutor(async_executor));
  EXPECT_THAT(lib_cpio_provider->GetCpuAsyncExecutorMember(), NotNull());

  EXPECT_SUCCESS(lib_cpio_provider->Stop());
  // Can be stopped outside LibCpioProvider.
  EXPECT_SUCCESS(async_executor->Stop());
}

TEST(LibCpioProviderTest, SetIoAsyncExecutor) {
  auto lib_cpio_provider = make_unique<MockLibCpioProviderWithOverrides>();
  EXPECT_SUCCESS(lib_cpio_provider->Init());
  EXPECT_SUCCESS(lib_cpio_provider->Run());
  EXPECT_THAT(lib_cpio_provider->GetIoAsyncExecutorMember(), IsNull());

  shared_ptr<AsyncExecutorInterface> async_executor =
      make_shared<AsyncExecutor>(1, 2);
  EXPECT_SUCCESS(async_executor->Init());
  EXPECT_SUCCESS(async_executor->Run());
  EXPECT_SUCCESS(lib_cpio_provider->SetIoAsyncExecutor(async_executor));
  EXPECT_THAT(lib_cpio_provider->GetIoAsyncExecutorMember(), NotNull());

  EXPECT_SUCCESS(lib_cpio_provider->Stop());
  // Can be stopped outside LibCpioProvider.
  EXPECT_SUCCESS(async_executor->Stop());
}
}  // namespace google::scp::cpio::test
