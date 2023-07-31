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

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "core/interface/config_provider_interface.h"
#include "core/interface/http_server_interface.h"
#include "core/interface/transaction_manager_interface.h"
#include "cpio/client_providers/interface/metric_client_provider_interface.h"
#include "cpio/client_providers/metric_client_provider/interface/aggregate_metric_interface.h"
#include "pbs/interface/budget_key_provider_interface.h"
#include "pbs/interface/front_end_service_interface.h"
#include "pbs/interface/type_def.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::pbs {
/**
 * @brief To provide health check functionality, a health service will return
 * success execution result to all health inquiries.
 */
class HealthService : public core::ServiceInterface {
 public:
  explicit HealthService(
      std::shared_ptr<core::HttpServerInterface>& http_server,
      std::shared_ptr<core::ConfigProviderInterface>& config_provider)
      : http_server_(http_server), config_provider_(config_provider) {}

  core::ExecutionResult Init() noexcept override;
  core::ExecutionResult Run() noexcept override;
  core::ExecutionResult Stop() noexcept override;

 protected:
  HealthService() {}

  /**
   * @brief Returns success if the instance if healthy.
   *
   * @param http_context The http context of the operation.
   * @return core::ExecutionResult The execution result of the operation.
   */
  core::ExecutionResult CheckHealth(
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_context) noexcept;

  /**
   * @brief Check the memory and storage usage to determine heath.
   *
   * @return core::ExecutionResult
   */
  virtual core::ExecutionResult CheckMemoryAndStorageUsage() noexcept;

  /**
   * @brief Whether to perform a memory and storage usage check and count
   * results into health check.
   *
   * @return true
   * @return false
   */
  bool PerformMemoryAndStorageUsageCheck() noexcept;

  /**
   * @brief Get the path to the meminfo file.
   *
   * @return std::string
   */
  virtual std::string GetMemInfoFilePath() noexcept;

  /**
   * @brief Get the percentage of memory that is currently being used in this
   * system.
   *
   * @return core::ExecutionResultOr<int>
   */
  core::ExecutionResultOr<int> GetMemoryUsagePercentage() noexcept;

  /**
   * @brief Get the File System Space Info object
   *
   * @param directory
   * @return ExecutionResultOr<std::filesystem::space_info>
   */
  virtual core::ExecutionResultOr<std::filesystem::space_info>
  GetFileSystemSpaceInfo(std::string directory) noexcept;

  /**
   * @brief Get the percentage of storage that is currently being used in this
   * directory or mount point.
   *
   * @return core::ExecutionResultOr<int>
   */
  core::ExecutionResultOr<int> GetFileSystemStorageUsagePercentage(
      const std::string& directory) noexcept;

  /// An instance of the http server.
  std::shared_ptr<core::HttpServerInterface> http_server_;
  /// An instance of the config provider.
  std::shared_ptr<core::ConfigProviderInterface> config_provider_;
};

}  // namespace google::scp::pbs
