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
#include <memory>
#include <string>

#include "absl/base/nullability.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/interface/http_server_interface.h"
#include "cc/public/core/interface/execution_result.h"
#include "opentelemetry/metrics/async_instruments.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/observer_result.h"

namespace privacy_sandbox::pbs {
/**
 * @brief To provide health check functionality, a health service will return
 * success execution result to all health inquiries.
 */
class HealthService : public pbs_common::ServiceInterface {
 public:
  HealthService(
      const std::shared_ptr<pbs_common::HttpServerInterface>& http_server,
      const std::shared_ptr<pbs_common::ConfigProviderInterface>&
          config_provider,
      const std::shared_ptr<pbs_common::AsyncExecutorInterface>& async_executor)
      : http_server_(http_server),
        config_provider_(config_provider),
        async_executor_(async_executor) {}

  ~HealthService();

  pbs_common::ExecutionResult Init() noexcept override;
  pbs_common::ExecutionResult Run() noexcept override;
  pbs_common::ExecutionResult Stop() noexcept override;

 protected:
  // Callback to be used with an OTel ObservableInstrument.
  static void ObserveMemoryUsageCallback(
      opentelemetry::metrics::ObserverResult observer_result,
      absl::Nonnull<HealthService*> self_ptr);

  // Callback to be used with an OTel ObservableInstrument.
  static void ObserveFileSystemStorageUsageCallback(
      opentelemetry::metrics::ObserverResult observer_result,
      absl::Nonnull<HealthService*> self_ptr);

  HealthService() {}

  /**
   * @brief Returns success if the instance if healthy.
   *
   * @param http_context The http context of the operation.
   * @return pbs_common::ExecutionResult The execution result
   * of the operation.
   */
  pbs_common::ExecutionResult CheckHealth(
      pbs_common::AsyncContext<pbs_common::HttpRequest,
                               pbs_common::HttpResponse>&
          http_context) noexcept;

  /**
   * @brief Check the memory and storage usage to determine heath.
   *
   * @return pbs_common::ExecutionResult
   */
  virtual pbs_common::ExecutionResult CheckMemoryAndStorageUsage() noexcept;

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
   * @return pbs_common::ExecutionResultOr<int>
   */
  pbs_common::ExecutionResultOr<int> GetMemoryUsagePercentage() noexcept;

  /**
   * @brief Get the File System Space Info object
   *
   * @param directory
   * @return ExecutionResultOr<std::filesystem::space_info>
   */
  virtual pbs_common::ExecutionResultOr<std::filesystem::space_info>
  GetFileSystemSpaceInfo(std::string directory) noexcept;

  /**
   * @brief Get the percentage of storage that is currently being used in this
   * directory or mount point.
   *
   * @return pbs_common::ExecutionResultOr<int>
   */
  pbs_common::ExecutionResultOr<int> GetFileSystemStorageUsagePercentage(
      const std::string& directory) noexcept;

  // An instance of the http server.
  std::shared_ptr<pbs_common::HttpServerInterface> http_server_;
  // An instance of the config provider.
  std::shared_ptr<pbs_common::ConfigProviderInterface> config_provider_;
  // Async executor instance.
  std::shared_ptr<pbs_common::AsyncExecutorInterface> async_executor_;
  // The OpenTelemetry Meter used for creating and managing metrics.
  std::shared_ptr<opentelemetry::metrics::Meter> meter_;
  // The OpenTelemetry Instrument for instance memory usage.
  std::shared_ptr<opentelemetry::metrics::ObservableInstrument>
      memory_usage_instrument_;
  // The OpenTelemetry Instrument for instance file system storage usage.
  std::shared_ptr<opentelemetry::metrics::ObservableInstrument>
      filesystem_storage_usage_instrument_;
};

}  // namespace privacy_sandbox::pbs
