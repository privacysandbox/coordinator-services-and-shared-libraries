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

#include "cpio/client_providers/metric_client_provider/interface/timeseries_metric_interface.h"
#include "cpio/client_providers/metric_client_provider/interface/type_def.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers::mock {
class MockTimeSeriesMetric : public TimeSeriesMetricInterface {
 public:
  MockSimpleMetric() {}

  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  }

  void Increment() noexcept override {}

  void IncrementBy(uint64_t incrementer) noexcept override {}

  void Decrement() noexcept override {}

  void DecrementBy(uint64_t decrementer) noexcept override {}

  void Push(const std::shared_ptr<TimeEvent>& time_event) noexcept override {}
};
}  // namespace google::scp::cpio::client_providers::mock
