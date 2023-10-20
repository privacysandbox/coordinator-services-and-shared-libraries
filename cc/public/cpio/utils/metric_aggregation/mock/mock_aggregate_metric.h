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
#include <mutex>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/utils/metric_aggregation/interface/aggregate_metric_interface.h"

namespace google::scp::cpio {
class MockAggregateMetric : public AggregateMetricInterface {
 public:
  MockAggregateMetric() {}

  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Increment(
      const std::string& event_code) noexcept override {
    return IncrementBy(1, event_code);
  }

  core::ExecutionResult IncrementBy(
      uint64_t value, const std::string& event_code) noexcept override {
    std::unique_lock lock(mutex_);
    metric_count_map_[event_code] = GetCounter(event_code) + value;
    return core::SuccessExecutionResult();
  }

  size_t GetCounter(const std::string& event_code = std::string()) {
    if (event_code.empty() || !metric_count_map_.contains(event_code)) {
      return 0;
    }
    return metric_count_map_.at(event_code);
  }

 private:
  std::mutex mutex_;
  absl::flat_hash_map<std::string, size_t> metric_count_map_;
};
}  // namespace google::scp::cpio
