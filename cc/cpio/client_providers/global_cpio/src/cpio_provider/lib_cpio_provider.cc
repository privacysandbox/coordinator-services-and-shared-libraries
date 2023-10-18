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

#include "lib_cpio_provider.h"

#include <memory>

#include "core/async_executor/src/async_executor.h"
#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/curl_client/src/http1_curl_client.h"
#include "core/http2_client/src/http2_client.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/message_router_interface.h"
#include "core/interface/service_interface.h"
#include "core/message_router/src/message_router.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cpio/client_providers/interface/cloud_initializer_interface.h"
#include "cpio/client_providers/interface/cpio_provider_interface.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::Http1CurlClient;
using google::scp::core::HttpClient;
using google::scp::core::HttpClientInterface;
using google::scp::core::MessageRouter;
using google::scp::core::MessageRouterInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_LIB_CPIO_ROVIDER_CPU_ASYNC_EXECUTOR_ALREADY_EXISTS;
using google::scp::core::errors::
    SC_LIB_CPIO_ROVIDER_IO_ASYNC_EXECUTOR_ALREADY_EXISTS;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;

static constexpr char kLibCpioProvider[] = "LibCpioProvider";
static const size_t kThreadPoolThreadCount = 2;
static const size_t kThreadPoolQueueSize = 100000;
static const size_t kIOThreadPoolThreadCount = 2;
static const size_t kIOThreadPoolQueueSize = 100000;

namespace google::scp::cpio::client_providers {
core::ExecutionResult LibCpioProvider::Init() noexcept {
  if (cpio_options_->cloud_init_option == CloudInitOption::kInitInCpio) {
    cloud_initializer_ = CloudInitializerFactory::Create();
    auto execution_result = cloud_initializer_->Init();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to init cloud initializer.");
      return execution_result;
    }
  }
  return SuccessExecutionResult();
}

core::ExecutionResult LibCpioProvider::Run() noexcept {
  if (cpio_options_->cloud_init_option == CloudInitOption::kInitInCpio) {
    auto execution_result = cloud_initializer_->Run();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to run cloud initializer.");
      return execution_result;
    }
    cloud_initializer_->InitCloud();
  }
  return SuccessExecutionResult();
}

core::ExecutionResult LibCpioProvider::Stop() noexcept {
  if (instance_client_provider_) {
    auto execution_result = instance_client_provider_->Stop();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to stop instance client provider.");
      return execution_result;
    }
  }

  if (auth_token_provider_) {
    auto execution_result = auth_token_provider_->Stop();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to stop auth token provider.");
      return execution_result;
    }
  }

  if (http2_client_) {
    auto execution_result = http2_client_->Stop();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to stop http2 client.");
      return execution_result;
    }
  }

  if (http1_client_) {
    auto execution_result = http1_client_->Stop();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to stop http1 client.");
      return execution_result;
    }
  }

  if (!external_cpu_async_executor_is_set_ && cpu_async_executor_) {
    auto execution_result = cpu_async_executor_->Stop();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to stop CPU async executor.");
      return execution_result;
    }
  }

  if (!external_io_asycn_executor_is_set_ && io_async_executor_) {
    auto execution_result = io_async_executor_->Stop();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to stop IO async executor.");
      return execution_result;
    }
  }

  if (cpio_options_->cloud_init_option == CloudInitOption::kInitInCpio) {
    cloud_initializer_->ShutdownCloud();
    auto execution_result = cloud_initializer_->Stop();
    if (!execution_result.Successful()) {
      SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
                "Failed to stop cloud initializer.");
      return execution_result;
    }
  }

  return SuccessExecutionResult();
}

ExecutionResult LibCpioProvider::GetHttpClient(
    std::shared_ptr<HttpClientInterface>& http2_client) noexcept {
  if (http2_client_) {
    http2_client = http2_client_;
    return SuccessExecutionResult();
  }

  shared_ptr<AsyncExecutorInterface> cpu_async_executor;
  auto execution_result =
      LibCpioProvider::GetCpuAsyncExecutor(cpu_async_executor);
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to get asynce executor.");
    return execution_result;
  }

  http2_client_ = make_shared<HttpClient>(cpu_async_executor);
  execution_result = http2_client_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to initialize http2 client.");
    return execution_result;
  }

  execution_result = http2_client_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to run http2 client.");
    return execution_result;
  }
  http2_client = http2_client_;
  return SuccessExecutionResult();
}

ExecutionResult LibCpioProvider::GetHttp1Client(
    std::shared_ptr<HttpClientInterface>& http1_client) noexcept {
  if (http1_client_) {
    http1_client = http1_client_;
    return SuccessExecutionResult();
  }

  shared_ptr<AsyncExecutorInterface> cpu_async_executor;
  auto execution_result =
      LibCpioProvider::GetCpuAsyncExecutor(cpu_async_executor);
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to get CPU async executor.");
    return execution_result;
  }

  shared_ptr<AsyncExecutorInterface> io_async_executor;
  execution_result = LibCpioProvider::GetIoAsyncExecutor(io_async_executor);
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to get IO async executor.");
    return execution_result;
  }

  http1_client_ =
      make_shared<Http1CurlClient>(cpu_async_executor, io_async_executor);
  execution_result = http1_client_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to initialize http1 client.");
    return execution_result;
  }

  execution_result = http1_client_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to run http1 client.");
    return execution_result;
  }
  http1_client = http1_client_;
  return SuccessExecutionResult();
}

ExecutionResult LibCpioProvider::SetCpuAsyncExecutor(
    const std::shared_ptr<AsyncExecutorInterface>&
        cpu_async_executor) noexcept {
  if (cpu_async_executor_) {
    return FailureExecutionResult(
        SC_LIB_CPIO_ROVIDER_CPU_ASYNC_EXECUTOR_ALREADY_EXISTS);
  }
  cpu_async_executor_ = cpu_async_executor;
  external_cpu_async_executor_is_set_ = true;

  return SuccessExecutionResult();
}

ExecutionResult LibCpioProvider::SetIoAsyncExecutor(
    const std::shared_ptr<AsyncExecutorInterface>& io_async_executor) noexcept {
  if (io_async_executor_) {
    return FailureExecutionResult(
        SC_LIB_CPIO_ROVIDER_IO_ASYNC_EXECUTOR_ALREADY_EXISTS);
  }
  io_async_executor_ = io_async_executor;
  external_io_asycn_executor_is_set_ = true;

  return SuccessExecutionResult();
}

ExecutionResult LibCpioProvider::GetCpuAsyncExecutor(
    std::shared_ptr<AsyncExecutorInterface>& cpu_async_executor) noexcept {
  if (cpu_async_executor_) {
    cpu_async_executor = cpu_async_executor_;
    return SuccessExecutionResult();
  }

  cpu_async_executor_ =
      make_shared<AsyncExecutor>(kThreadPoolThreadCount, kThreadPoolQueueSize);
  auto execution_result = cpu_async_executor_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to initialize async executor.");
    return execution_result;
  }

  execution_result = cpu_async_executor_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to run async executor.");
    return execution_result;
  }
  cpu_async_executor = cpu_async_executor_;
  return SuccessExecutionResult();
}

ExecutionResult LibCpioProvider::GetIoAsyncExecutor(
    std::shared_ptr<AsyncExecutorInterface>& io_async_executor) noexcept {
  if (io_async_executor_) {
    io_async_executor = io_async_executor_;
    return SuccessExecutionResult();
  }

  io_async_executor_ = make_shared<AsyncExecutor>(kIOThreadPoolThreadCount,
                                                  kIOThreadPoolQueueSize);
  auto execution_result = io_async_executor_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to initialize IO async executor.");
    return execution_result;
  }

  execution_result = io_async_executor_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to run IO async executor.");
    return execution_result;
  }
  io_async_executor = io_async_executor_;
  return SuccessExecutionResult();
}

ExecutionResult LibCpioProvider::GetInstanceClientProvider(
    shared_ptr<InstanceClientProviderInterface>&
        instance_client_provider) noexcept {
  if (instance_client_provider_) {
    instance_client_provider = instance_client_provider_;
    return SuccessExecutionResult();
  }

  shared_ptr<AuthTokenProviderInterface> auth_token_provider;
  auto execution_result =
      LibCpioProvider::GetAuthTokenProvider(auth_token_provider);
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to get auth token provider.");
    return execution_result;
  }

  shared_ptr<HttpClientInterface> http1_client;
  execution_result = LibCpioProvider::GetHttp1Client(http1_client);
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to get Http1 client.");
    return execution_result;
  }

  shared_ptr<HttpClientInterface> http2_client;
  execution_result = LibCpioProvider::GetHttpClient(http2_client);
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to get Http2 client.");
    return execution_result;
  }

  shared_ptr<AsyncExecutorInterface> cpu_async_executor;
  execution_result = LibCpioProvider::GetCpuAsyncExecutor(cpu_async_executor);
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to get cpu async executor.");
    return execution_result;
  }

  shared_ptr<AsyncExecutorInterface> io_async_executor;
  execution_result = LibCpioProvider::GetIoAsyncExecutor(io_async_executor);
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to get io async executor.");
    return execution_result;
  }

  instance_client_provider_ = InstanceClientProviderFactory::Create(
      auth_token_provider, http1_client, http2_client, cpu_async_executor,
      io_async_executor);
  execution_result = instance_client_provider_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to initialize instance client provider.");
    return execution_result;
  }

  execution_result = instance_client_provider_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to run instance client provider.");
    return execution_result;
  }
  instance_client_provider = instance_client_provider_;
  return SuccessExecutionResult();
}

shared_ptr<RoleCredentialsProviderInterface>
LibCpioProvider::CreateRoleCredentialsProvider(
    const shared_ptr<InstanceClientProviderInterface>& instance_client_provider,
    const shared_ptr<AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) noexcept {
  return RoleCredentialsProviderFactory::Create(
      make_shared<RoleCredentialsProviderOptions>(), instance_client_provider,
      cpu_async_executor, io_async_executor);
}

ExecutionResult LibCpioProvider::GetRoleCredentialsProvider(
    shared_ptr<RoleCredentialsProviderInterface>&
        role_credentials_provider) noexcept {
  if (role_credentials_provider) {
    role_credentials_provider = role_credentials_provider_;
    return SuccessExecutionResult();
  }

  shared_ptr<AsyncExecutorInterface> cpu_async_executor;
  auto execution_result =
      LibCpioProvider::GetCpuAsyncExecutor(cpu_async_executor);
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to get cpu async executor.");
    return execution_result;
  }

  shared_ptr<AsyncExecutorInterface> io_async_executor;
  execution_result = LibCpioProvider::GetIoAsyncExecutor(io_async_executor);
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to get io async executor.");
    return execution_result;
  }

  shared_ptr<InstanceClientProviderInterface> instance_client;
  execution_result = GetInstanceClientProvider(instance_client);
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to get instance client.");
    return execution_result;
  }

  role_credentials_provider_ = CreateRoleCredentialsProvider(
      instance_client, cpu_async_executor, io_async_executor);
  execution_result = role_credentials_provider_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to initialize role credential provider.");
    return execution_result;
  }

  execution_result = role_credentials_provider_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to run role credential provider.");
    return execution_result;
  }
  role_credentials_provider = role_credentials_provider_;
  return SuccessExecutionResult();
}

ExecutionResult LibCpioProvider::GetAuthTokenProvider(
    shared_ptr<AuthTokenProviderInterface>& auth_token_provider) noexcept {
  if (auth_token_provider) {
    auth_token_provider = auth_token_provider_;
    return SuccessExecutionResult();
  }

  shared_ptr<HttpClientInterface> http1_client;
  auto execution_result = LibCpioProvider::GetHttp1Client(http1_client);
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to get Http1 client.");
    return execution_result;
  }

  auth_token_provider_ = AuthTokenProviderFactory::Create(http1_client);
  execution_result = auth_token_provider_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to initialize auth token provider.");
    return execution_result;
  }

  execution_result = auth_token_provider_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kLibCpioProvider, kZeroUuid, execution_result,
              "Failed to run role  auth token provider.");
    return execution_result;
  }
  auth_token_provider = auth_token_provider_;
  return SuccessExecutionResult();
}

#ifndef TEST_CPIO
unique_ptr<CpioProviderInterface> CpioProviderFactory::Create(
    const shared_ptr<CpioOptions>& options) {
  return make_unique<LibCpioProvider>(options);
}
#endif
}  // namespace google::scp::cpio::client_providers
