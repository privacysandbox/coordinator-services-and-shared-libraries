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

#include "public/core/interface/execution_result.h"

#include "aggregate_metric_interface.h"
#include "type_def.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Represents a time series metric. It records the counters and
 * accumulative time of one defined metric, and pushes the average execution
 * time and counters in set period to cloud with a scheduled async executor.
 */
class TimeSeriesMetricInterface : public core::ServiceInterface {
 public:
  virtual ~TimeSeriesMetricInterface() = default;

  /// Increments the counter with default incrementer value of one.
  virtual void Increment() noexcept = 0;

  /**
   * @brief Increments the counter with input incrementer.
   *
   * @param incrementer value of incrementer.
   */
  virtual void IncrementBy(uint64_t incrementer) noexcept = 0;

  /// Decrements the counter with default decrementer value of one.
  virtual void Decrement() noexcept = 0;

  /**
   * @brief Decrements the counter with input decrementer.
   *
   * @param incrementer value of decrementer.
   */
  virtual void DecrementBy(uint64_t decrementer) noexcept = 0;

  /**
   * @brief Pushes a time_event which accumulates the execution time and
   * counters for the metric.
   *
   * @param time_event The time_event contains the start, end and
   * difference time of one event.
   */
  virtual void Push(const std::shared_ptr<TimeEvent>& time_event) noexcept = 0;
};
}  // namespace google::scp::cpio::client_providers
