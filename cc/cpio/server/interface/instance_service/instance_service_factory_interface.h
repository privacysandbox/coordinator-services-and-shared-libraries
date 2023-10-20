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
#include <string>

#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/instance_client_provider_interface.h"

namespace google::scp::cpio {

struct InstanceServiceFactoryOptions {
  virtual ~InstanceServiceFactoryOptions() = default;

  std::string cpu_async_executor_thread_count_config_label;
  std::string cpu_async_executor_queue_cap_config_label;
  std::string io_async_executor_thread_count_config_label;
  std::string io_async_executor_queue_cap_config_label;
};

/**
 * @brief Platform specific factory interface to provide platform specific
 * clients to InstanceService
 *
 */
class InstanceServiceFactoryInterface : public core::ServiceInterface {
 public:
  virtual ~InstanceServiceFactoryInterface() = default;

  /**
   * @brief Creates InstanceClient.
   *
   * @return std::shared_ptr<client_providers::InstanceClientProviderInterface>
   * created InstanceClient.
   */
  virtual std::shared_ptr<client_providers::InstanceClientProviderInterface>
  CreateInstanceClient() noexcept = 0;

  /**
   * @brief Gets Http1Client.
   *
   * @return std::shared_ptr<core::HttpClientInterface>
   * the Http1Client.
   */
  virtual std::shared_ptr<core::HttpClientInterface>
  GetHttp1Client() noexcept = 0;

  /**
   * @brief Gets Http2Client.
   *
   * @return core::ExecutionResultOr<std::shared_ptr<core::HttpClientInterface>>
   * the Http2Client.
   */
  virtual core::ExecutionResultOr<std::shared_ptr<core::HttpClientInterface>>
  GetHttp2Client() noexcept = 0;

  /**
   * @brief Gets CpuAsyncExecutor.
   *
   * @return std::shared_ptr<core::AsyncExecutorInterface> CpuAsyncExecutor.
   */
  virtual std::shared_ptr<core::AsyncExecutorInterface>
  GetCpuAsynceExecutor() noexcept = 0;

  /**
   * @brief Gets IoAsyncExecutor.
   *
   * @return std::shared_ptr<core::AsyncExecutorInterface> IoAsyncExecutor.
   */
  virtual std::shared_ptr<core::AsyncExecutorInterface>
  GetIoAsynceExecutor() noexcept = 0;
};
}  // namespace google::scp::cpio
