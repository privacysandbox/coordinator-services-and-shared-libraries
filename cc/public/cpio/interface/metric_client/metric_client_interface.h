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

#ifndef SCP_CPIO_INTERFACE_METRIC_CLIENT_INTERFACE_H_
#define SCP_CPIO_INTERFACE_METRIC_CLIENT_INTERFACE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/interface/async_context.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"

#include "type_def.h"

namespace google::scp::cpio {
/// Represents the metric object.
struct Metric {
  /// Metric name.
  MetricName name;

  /// The value of the metric data point. Inside it, it should be double type.
  MetricValue value;

  /// The unit of the metric data point.
  MetricUnit unit;

  /// A set of key-value pairs.
  MetricLabels labels;

  /// The time the metric data was received in milliseconds. This is an optional
  /// field and the default value is current time.
  Timestamp timestamp_in_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
};

/// Represents all fields required to record custom metrics.
struct PutMetricsRequest {
  std::vector<Metric> metrics;
};

/// The response object of recording custom metrics.
struct PutMetricsResponse {};

/**
 * @brief Interface responsible for recording custom metrics.
 *
 * Use Create to create the MetricClient. Call Init and Run before actually use
 * it, and call Stop when finish using it.
 */
class MetricClientInterface : public core::ServiceInterface {
 public:
  /**
   * @brief Records custom metrics on Cloud.
   *
   * @param request request for the call.
   * @param callback callback will be triggered when the call completes
   * including when the call fails.
   * @return core::ExecutionResult scheduling result returned synchronously.
   */
  virtual core::ExecutionResult PutMetrics(
      PutMetricsRequest request,
      Callback<PutMetricsResponse> callback) noexcept = 0;
};

/// Factory to create MetricClient.
class MetricClientFactory {
 public:
  /**
   * @brief Creates MetricClient.
   *
   * @param options configurations for MetricClient.
   * @return std::unique_ptr<MetricClientInterface> MetricClient object.
   */
  static std::unique_ptr<MetricClientInterface> Create(
      MetricClientOptions options);
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_METRIC_CLIENT_INTERFACE_H_
