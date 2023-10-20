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

#pragma once

#include <memory>

#include "core/interface/async_executor_interface.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/message_router_interface.h"
#include "core/interface/service_interface.h"
#include "core/message_router/src/message_router.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Provides all required global objects. This class is not thread-safe,
 * but it will only be used for client initialization where only one main
 * process is running.
 *
 */
class CpioProviderInterface : public core::ServiceInterface {
 public:
  /**
   * @brief Sets the global Async Executor.
   *
   * @param cpu_async_executor the CPU Async Executor.
   * @return core::ExecutionResult get result.
   */
  virtual core::ExecutionResult SetCpuAsyncExecutor(
      const std::shared_ptr<core::AsyncExecutorInterface>&
          cpu_async_executor) noexcept = 0;

  /**
   * @brief Sets the global IO Async Executor.
   *
   * @param io_async_executor the IO Async Executor.
   * @return core::ExecutionResult get result.
   */
  virtual core::ExecutionResult SetIoAsyncExecutor(
      const std::shared_ptr<core::AsyncExecutorInterface>&
          io_async_executor) noexcept = 0;

  /**
   * @brief Gets the global Async Executor. Only create it when it is
   * needed.
   *
   * @param cpu_async_executor the CPU Async Executor.
   * @return core::ExecutionResult get result.
   */
  virtual core::ExecutionResult GetCpuAsyncExecutor(
      std::shared_ptr<core::AsyncExecutorInterface>&
          cpu_async_executor) noexcept = 0;

  /**
   * @brief Gets the global IO Async Executor. Only create it when it is
   * needed.
   *
   * @param io_async_executor the IO Async Executor.
   * @return core::ExecutionResult get result.
   */
  virtual core::ExecutionResult GetIoAsyncExecutor(
      std::shared_ptr<core::AsyncExecutorInterface>&
          io_async_executor) noexcept = 0;

  /**
   * @brief Get the Http2 Client object. Only create it when it is needed.
   * TODO: rename to GetHttp2Client.
   *
   * @param http_client output Http2 Client
   * @return core::ExecutionResult get result.
   */
  virtual core::ExecutionResult GetHttpClient(
      std::shared_ptr<core::HttpClientInterface>& http2_client) noexcept = 0;

  /**
   * @brief Get the Http1 Client object. Only create it when it is needed.
   *
   * @param http_client output Http1 Client
   * @return core::ExecutionResult get result.
   */
  virtual core::ExecutionResult GetHttp1Client(
      std::shared_ptr<core::HttpClientInterface>& http1_client) noexcept = 0;

  /**
   * @brief Gets the InstanceClientProvider.
   *
   * @param instance_client output InstanceClientProvider.
   * @return core::ExecutionResult get result.
   */
  virtual core::ExecutionResult GetInstanceClientProvider(
      std::shared_ptr<InstanceClientProviderInterface>&
          instance_client) noexcept = 0;

  /**
   * @brief Gets the Role Credentials Provider object when it is needed.
   *
   * @param credentials_provider output role credentials provider.
   * @return core::ExecutionResult get result.
   */
  virtual core::ExecutionResult GetRoleCredentialsProvider(
      std::shared_ptr<RoleCredentialsProviderInterface>&
          role_credentials_provider) noexcept = 0;

  virtual core::ExecutionResult GetAuthTokenProvider(
      std::shared_ptr<AuthTokenProviderInterface>&
          auth_token_provider) noexcept = 0;
};

/// Factory to create CpioProvider.
class CpioProviderFactory {
 public:
  /**
   * @brief Creats CpioProvider.
   *
   * @return std::unique_ptr<CpioProviderInterface> CpioProvider.
   */
  static std::unique_ptr<CpioProviderInterface> Create(
      const std::shared_ptr<CpioOptions>& options);
};
}  // namespace google::scp::cpio::client_providers
