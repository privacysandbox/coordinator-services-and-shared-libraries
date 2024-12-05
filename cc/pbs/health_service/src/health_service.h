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

#include "absl/base/nullability.h"
#include "cc/core/interface/async_context.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/config_provider_interface.h"
#include "cc/core/interface/http_server_interface.h"
#include "cc/core/interface/transaction_manager_interface.h"
#include "cc/core/telemetry/src/metric/metric_router.h"
#include "cc/cpio/client_providers/interface/metric_client_provider_interface.h"
#include "cc/pbs/interface/budget_key_provider_interface.h"
#include "cc/pbs/interface/front_end_service_interface.h"
#include "cc/pbs/interface/type_def.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/cpio/interface/metric_client/metric_client_interface.h"
#include "cc/public/cpio/utils/metric_aggregation/interface/simple_metric_interface.h"
#include "opentelemetry/metrics/async_instruments.h"
#include "opentelemetry/metrics/meter.h"
#include "opentelemetry/metrics/observer_result.h"

namespace google::scp::pbs {
/**
 * @brief To provide health check functionality, a health service will return
 * success execution result to all health inquiries.
 */
class HealthService : public core::ServiceInterface {
 public:
  HealthService(
      const std::shared_ptr<core::HttpServerInterface>& http_server,
      const std::shared_ptr<core::ConfigProviderInterface>& config_provider,
      const std::shared_ptr<core::AsyncExecutorInterface>& async_executor,
      const std::shared_ptr<cpio::MetricClientInterface>& metric_client)
      : http_server_(http_server),
        config_provider_(config_provider),
        async_executor_(async_executor),
        metric_client_(metric_client),
        last_metric_push_steady_ns_timestamp_(0) {}

  ~HealthService();

  core::ExecutionResult Init() noexcept override;
  core::ExecutionResult Run() noexcept override;
  core::ExecutionResult Stop() noexcept override;

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

  // An instance of the http server.
  std::shared_ptr<core::HttpServerInterface> http_server_;
  // An instance of the config provider.
  std::shared_ptr<core::ConfigProviderInterface> config_provider_;
  // Async executor instance.
  std::shared_ptr<core::AsyncExecutorInterface> async_executor_;
  // Metric client instance for custom metric recording.
  std::shared_ptr<cpio::MetricClientInterface> metric_client_;
  // The simple metric instance for instance memory usage.
  std::shared_ptr<cpio::SimpleMetricInterface> instance_memory_usage_metric_;
  // The simple metric instance for instance FS usage.
  std::shared_ptr<cpio::SimpleMetricInterface>
      instance_filesystem_storage_usage_metric_;
  // Metric should not be pushed too quickly, so keep track of the last push.
  std::chrono::nanoseconds last_metric_push_steady_ns_timestamp_;
  // The OpenTelemetry Meter used for creating and managing metrics.
  std::shared_ptr<opentelemetry::metrics::Meter> meter_;
  // The OpenTelemetry Instrument for instance memory usage.
  std::shared_ptr<opentelemetry::metrics::ObservableInstrument>
      memory_usage_instrument_;
  // The OpenTelemetry Instrument for instance file system storage usage.
  std::shared_ptr<opentelemetry::metrics::ObservableInstrument>
      filesystem_storage_usage_instrument_;

 private:
  // Initialize MetricClient.
  //
  core::ExecutionResult InitMetricClientInterface();
};

}  // namespace google::scp::pbs
