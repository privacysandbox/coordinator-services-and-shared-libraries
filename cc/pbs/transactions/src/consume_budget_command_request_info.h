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

#include <optional>

#include "pbs/interface/type_def.h"

namespace google::scp::pbs {
/**
 * @brief ConsumeBudgetCommandRequestInfo
 * Structure to convey consumption request details of a budget key to a
 * transaction command.
 */
struct ConsumeBudgetCommandRequestInfo {
  ConsumeBudgetCommandRequestInfo() = delete;

  ConsumeBudgetCommandRequestInfo(const ConsumeBudgetCommandRequestInfo&) =
      default;

  ConsumeBudgetCommandRequestInfo& operator=(
      ConsumeBudgetCommandRequestInfo&&) = default;

  ConsumeBudgetCommandRequestInfo& operator=(
      const ConsumeBudgetCommandRequestInfo&) = default;

  ConsumeBudgetCommandRequestInfo(ConsumeBudgetCommandRequestInfo&) = default;

  ConsumeBudgetCommandRequestInfo(TimeBucket time_bucket,
                                  TokenCount token_count)
      : time_bucket(time_bucket), token_count(token_count) {}

  ConsumeBudgetCommandRequestInfo(TimeBucket time_bucket,
                                  TokenCount token_count, size_t request_index)
      : time_bucket(time_bucket),
        token_count(token_count),
        request_index(request_index) {}

  /// @brief Time bucket at which the budget needs to be consumed
  TimeBucket time_bucket = 0;

  /// @brief Number of tokens to be consumed on the time bucket
  TokenCount token_count = 0;

  /// @brief Optional field to indicate the index/position of this budget
  /// corresponding to the array of budgets provided by client. This is useful
  /// for reporting purposes.
  /// TODO: This needs to be serialized when transaction commands are serialized
  std::optional<size_t> request_index;

  bool operator==(const ConsumeBudgetCommandRequestInfo& other) const {
    if ((time_bucket != other.time_bucket) ||
        (token_count != other.token_count)) {
      return false;
    }
    bool has_optional_fields =
        (request_index.has_value() && other.request_index.has_value());
    return !has_optional_fields || request_index == *other.request_index;
  }
};
}  // namespace google::scp::pbs
