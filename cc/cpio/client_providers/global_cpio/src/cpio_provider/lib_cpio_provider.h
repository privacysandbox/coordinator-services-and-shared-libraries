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

#include "core/async_executor/src/async_executor.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/message_router_interface.h"
#include "core/message_router/src/message_router.h"
#include "cpio/client_providers/interface/auth_token_provider_interface.h"
#include "cpio/client_providers/interface/cloud_initializer_interface.h"
#include "cpio/client_providers/interface/cpio_provider_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc CpioProviderInterface
 * Provides global objects for native library mode.
 */
class LibCpioProvider : public CpioProviderInterface {
 public:
  explicit LibCpioProvider(const std::shared_ptr<CpioOptions>& options)
      : cpio_options_(options),
        cloud_initializer_(nullptr),
        external_cpu_async_executor_is_set_(false),
        external_io_asycn_executor_is_set_(false) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult SetCpuAsyncExecutor(
      const std::shared_ptr<core::AsyncExecutorInterface>&
          cpu_async_executor) noexcept override;

  core::ExecutionResult SetIoAsyncExecutor(
      const std::shared_ptr<core::AsyncExecutorInterface>&
          io_async_executor) noexcept override;

  core::ExecutionResult GetCpuAsyncExecutor(
      std::shared_ptr<core::AsyncExecutorInterface>&
          cpu_async_executor) noexcept override;

  core::ExecutionResult GetIoAsyncExecutor(
      std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor) noexcept
      override;

  core::ExecutionResult GetHttpClient(
      std::shared_ptr<core::HttpClientInterface>& http_client) noexcept
      override;

  core::ExecutionResult GetHttp1Client(
      std::shared_ptr<core::HttpClientInterface>& http1_client) noexcept
      override;

  core::ExecutionResult GetInstanceClientProvider(
      std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider) noexcept override;

  core::ExecutionResult GetRoleCredentialsProvider(
      std::shared_ptr<RoleCredentialsProviderInterface>&
          role_credentials_provider) noexcept override;

  core::ExecutionResult GetAuthTokenProvider(
      std::shared_ptr<AuthTokenProviderInterface>& auth_token_provider) noexcept
      override;

 protected:
  /// Global CPIO options.
  std::shared_ptr<CpioOptions> cpio_options_;
  /// Global cloud initializer.
  std::shared_ptr<CloudInitializerInterface> cloud_initializer_;
  /// Global async executors.
  std::shared_ptr<core::AsyncExecutorInterface> cpu_async_executor_,
      io_async_executor_;
  /// Global http clients.
  std::shared_ptr<core::HttpClientInterface> http1_client_, http2_client_;
  /// Global instance client provider to fetch cloud metadata.
  std::shared_ptr<InstanceClientProviderInterface> instance_client_provider_;
  /// Global role credential provider.
  std::shared_ptr<RoleCredentialsProviderInterface> role_credentials_provider_;
  /// Global auth token provider.
  std::shared_ptr<AuthTokenProviderInterface> auth_token_provider_;

 private:
  virtual std::shared_ptr<RoleCredentialsProviderInterface>
  CreateRoleCredentialsProvider(
      const std::shared_ptr<InstanceClientProviderInterface>&
          instance_client_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>&
          io_async_executor) noexcept;

  bool external_cpu_async_executor_is_set_;
  bool external_io_asycn_executor_is_set_;
};
}  // namespace google::scp::cpio::client_providers
