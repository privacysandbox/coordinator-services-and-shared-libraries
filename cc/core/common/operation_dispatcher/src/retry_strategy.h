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

#include <chrono>
#include <cmath>

#include "core/interface/type_def.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::common {

/// Types of retry strategy
enum class RetryStrategyType {
  Linear = 0,
  Exponential = 1,
};

/// RetryStrategy options.
struct RetryStrategyOptions {
  RetryStrategyOptions() = delete;

  RetryStrategyOptions(RetryStrategyType retry_strategy_type,
                       TimeDuration delay_duration_ms,
                       size_t maximum_allowed_retry_count)
      : retry_strategy_type(retry_strategy_type),
        delay_duration_ms(delay_duration_ms),
        maximum_allowed_retry_count(maximum_allowed_retry_count) {}

  /// The type of the retry strategy, linear or exponential.
  const RetryStrategyType retry_strategy_type;

  /// The initial delay for any types of retries in milliseconds.
  const TimeDuration delay_duration_ms;

  /// The maximum number of retries that is allowed.
  const size_t maximum_allowed_retry_count;
};

/**
 * @brief A structure to represent retry strategy for operation. Linear and
 * Exponential retry strategies are supported.
 */
class RetryStrategy {
 public:
  RetryStrategy() = delete;

  /**
   * @brief Construct a new Retry Strategy object
   *
   * @param retry_strategy_type The type of the retry strategy, linear or
   * exponential.
   * @param delay_duration_ms The initial delay for any types of retries in
   * milliseconds.
   * @param maximum_allowed_retry_count The maximum number of retries that is
   * allowed.
   */
  RetryStrategy(RetryStrategyType retry_strategy_type,
                TimeDuration delay_duration_ms,
                size_t maximum_allowed_retry_count)
      : retry_strategy_type_(retry_strategy_type),
        delay_duration_ms_(delay_duration_ms),
        maximum_allowed_retry_count_(maximum_allowed_retry_count) {}

  explicit RetryStrategy(RetryStrategyOptions options)
      : retry_strategy_type_(options.retry_strategy_type),
        delay_duration_ms_(options.delay_duration_ms),
        maximum_allowed_retry_count_(options.maximum_allowed_retry_count) {}

  /**
   * @brief Get the back-off duration in milliseconds for any specific retry
   * count.
   *
   * @param retry_count The number of retries.
   * @return TimeDuration The back off duration in milliseconds.
   */
  TimeDuration GetBackOffDurationInMilliseconds(size_t retry_count) {
    if (retry_count == 0) {
      return 0;
    }

    switch (retry_strategy_type_) {
      case RetryStrategyType::Linear:
        return retry_count * delay_duration_ms_;
      case RetryStrategyType::Exponential:
      default:
        return pow(2, retry_count - 1) * delay_duration_ms_;
    }
  }

  /**
   * @brief Returns the maximum allowed retry count.
   *
   * @return size_t The maximum allowed retry count.
   */
  size_t GetMaximumAllowedRetryCount() { return maximum_allowed_retry_count_; }

 private:
  /// Retry strategy type.
  RetryStrategyType retry_strategy_type_;
  /// The delay in the back off time in milliseconds.
  TimeDuration delay_duration_ms_;
  /// Maximum allowed retry count for the retry strategy.
  size_t maximum_allowed_retry_count_;
};
}  // namespace google::scp::core::common
