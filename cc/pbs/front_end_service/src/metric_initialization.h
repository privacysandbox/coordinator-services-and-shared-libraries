// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CC_PBS_FRONT_END_SERVICE_SRC_METRIC_INITIALIZATION_H_
#define CC_PBS_FRONT_END_SERVICE_SRC_METRIC_INITIALIZATION_H_

#include <memory>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "cc/core/interface/async_executor_interface.h"
#include "cc/core/interface/type_def.h"
#include "cc/pbs/interface/type_def.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/cpio/interface/metric_client/metric_client_interface.h"
#include "cc/public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"

namespace google::scp::pbs {

inline constexpr std::array<absl::string_view, 7>
    kMetricInitializationMethodNames = {
        kMetricLabelBeginTransaction,    kMetricLabelPrepareTransaction,
        kMetricLabelCommitTransaction,   kMetricLabelAbortTransaction,
        kMetricLabelNotifyTransaction,   kMetricLabelEndTransaction,
        kMetricLabelGetStatusTransaction};

inline constexpr std::array<absl::string_view, 3>
    kMetricInitializationMetricNames = {
        kMetricNameRequests, kMetricNameClientErrors, kMetricNameServerErrors};

typedef absl::flat_hash_map<
    std::string,
    absl::flat_hash_map<std::string,
                        std::shared_ptr<cpio::AggregateMetricInterface>>>
    MetricsMap;

// Initializes TransactionMetric instances for a front end service.
class MetricInitialization {
 public:
  virtual ~MetricInitialization() = default;
  [[nodiscard]] virtual core::ExecutionResultOr<MetricsMap> Initialize(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<cpio::MetricClientInterface> metric_client,
      core::TimeDuration aggregated_metric_interval_ms) const noexcept = 0;
};

class MetricInitializationImplementation : public MetricInitialization {
 public:
  MetricInitializationImplementation() = default;
  // Not copyable or movable
  MetricInitializationImplementation(
      const MetricInitializationImplementation&) = delete;
  MetricInitializationImplementation& operator=(
      const MetricInitializationImplementation&) = delete;
  MetricInitializationImplementation(MetricInitializationImplementation&&) =
      delete;
  MetricInitializationImplementation& operator=(
      MetricInitializationImplementation&&) = delete;
  ~MetricInitializationImplementation() override = default;

  [[nodiscard]] core::ExecutionResultOr<MetricsMap> Initialize(
      std::shared_ptr<core::AsyncExecutorInterface> async_executor,
      std::shared_ptr<cpio::MetricClientInterface> metric_client,
      core::TimeDuration aggregated_metric_interval_ms) const noexcept override;
};
}  // namespace google::scp::pbs

#endif  // CC_PBS_FRONT_END_SERVICE_SRC_METRIC_INITIALIZATION_H_
