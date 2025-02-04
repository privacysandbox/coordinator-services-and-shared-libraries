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

#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>

#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "cc/pbs/interface/type_def.h"
#include "cc/public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::pbs::budget_key_timeframe_manager {
static constexpr TimeBucket kHoursPerDay = 24;
static constexpr core::Version kCurrentVersion = {.major = 1, .minor = 0};

class Serialization {
 public:
  /**
   * @brief Serializes 24 hours token per hour vector into string.
   *
   * @param hour_tokens A vector with size of 24. Each index represents the
   * amount of tokens available in a hour.
   * @param hour_token_in_time_group The serialized buffer of the vector.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult SerializeHourTokensInTimeGroup(
      const std::vector<TokenCount>& hour_tokens,
      std::string& hour_token_in_time_group) {
    if (hour_tokens.size() != kHoursPerDay) {
      return core::FailureExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA);
    }

    hour_token_in_time_group.clear();
    for (size_t i = 0; i < hour_tokens.size(); ++i) {
      hour_token_in_time_group += std::to_string(hour_tokens[i]);
      if (i < hour_tokens.size() - 1) {
        hour_token_in_time_group += " ";
      }
    }

    return core::SuccessExecutionResult();
  }

  /**
   * @brief Deserializes 24 hours token per hour vector from the provided
   * string.
   *
   * @param hour_token_in_time_group The serialized buffer of the vector.
   * @param hour_tokens A vector with size of 24. Each index represents the
   * amount of tokens available in a hour.
   * @return core::ExecutionResult The execution result of the operation.
   */
  static core::ExecutionResult DeserializeHourTokensInTimeGroup(
      const std::string& hour_token_in_time_group,
      std::vector<TokenCount>& hour_tokens) {
    std::vector<absl::string_view> tokens_per_hour =
        absl::StrSplit(hour_token_in_time_group, " ");

    if (tokens_per_hour.size() != kHoursPerDay) {
      return core::FailureExecutionResult(
          core::errors::SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA);
    }

    if (hour_tokens.size() != kHoursPerDay) {
      hour_tokens.resize(kHoursPerDay);
    }
    for (int i = 0; i < kHoursPerDay; ++i) {
      int32_t value;
      if (!absl::SimpleAtoi(tokens_per_hour[i], &value)) {
        return core::FailureExecutionResult(
            core::errors::
                SC_BUDGET_KEY_TIMEFRAME_MANAGER_CORRUPTED_KEY_METADATA);
      }
      hour_tokens[i] = value;
    }

    return core::SuccessExecutionResult();
  }
};
}  // namespace google::scp::pbs::budget_key_timeframe_manager
