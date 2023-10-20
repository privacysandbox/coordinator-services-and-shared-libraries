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

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/interface/async_context.h"
#include "google/cloud/future.h"
#include "google/cloud/monitoring/metric_client.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/metric_service/v1/metric_service.pb.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {

class GcpMetricClientUtils {
 public:
  /**
   * @brief Parses PutMetricsRequest to Gcp time series.
   *
   * @param record_metric_context the async context for MetricRecordRequest.
   * @param name_space Aws namespace.
   * @param time_series_list the reference of time series vector
   * @return core::ExecutionResult
   */
  static core::ExecutionResult ParseRequestToTimeSeries(
      core::AsyncContext<cmrt::sdk::metric_service::v1::PutMetricsRequest,
                         cmrt::sdk::metric_service::v1::PutMetricsResponse>&
          record_metric_context,
      const std::string& name_space,
      std::vector<monitoring::v3::TimeSeries>& time_series_list) noexcept;

  /**
   * @brief Adds gce instance MonitoredResource to TimeSeries list.
   *
   * @param project_id Gcp Project ID.
   * @param instance_id Gcp instance id.
   * @param instance_zone Gcp instance Compute Engine zone.
   * @param time_series_list the reference of time series vector.
   */
  static void AddResourceToTimeSeries(
      const std::string& project_id, const std::string& instance_id,
      const std::string& instance_zone,
      std::vector<monitoring::v3::TimeSeries>& time_series_list) noexcept;

  /**
   * @brief Constructs the project name for gcp CreateTimeSeriesRequest.
   *
   * @param project_id Gcp project id.
   * @return std::string Project name.
   */
  static std::string ConstructProjectName(const std::string& project_id);
};
}  // namespace google::scp::cpio::client_providers
