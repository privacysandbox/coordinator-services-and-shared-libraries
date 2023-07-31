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
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/common/serialization/src/serialization.h"
#include "pbs/budget_key_timeframe_manager/src/proto/budget_key_timeframe_manager.pb.h"
#include "pbs/interface/budget_key_timeframe_manager_interface.h"
#include "pbs/interface/type_def.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::pbs::budget_key_timeframe_manager {

class Utils {
  using days = std::chrono::duration<int64_t, std::ratio<86400>>;

 public:
  /**
   * @brief Gets the Time Bucket from the provided timestamp.
   *
   * @param timestamp The timestamp to calculate the time bucket from.
   * @return TimeBucket The time bucket from the timestamp.
   */
  static TimeBucket GetTimeBucket(core::Timestamp timestamp) noexcept {
    auto days_since_epoch = GetTimeGroup(timestamp);
    std::chrono::nanoseconds bucket_time_nano_seconds(timestamp);
    std::chrono::nanoseconds days_since_epoch_nano_seconds =
        days(days_since_epoch);

    return std::chrono::duration_cast<std::chrono::hours>(
               bucket_time_nano_seconds - days_since_epoch_nano_seconds)
        .count();
  }

  /**
   * @brief Gets Time Buckets that the provided timestamps belong to.
   *
   * @param timestamps The timestamps to calculate the time buckets from.
   * @return unordered_set<TimeBucket> Time buckets
   */
  static std::unordered_set<TimeBucket> GetUniqueTimeBuckets(
      const std::vector<core::Timestamp> timestamps) noexcept {
    std::unordered_set<TimeBucket> time_buckets;
    for (auto timestamp : timestamps) {
      time_buckets.insert(GetTimeBucket(timestamp));
    }
    return time_buckets;
  }

  /**
   * @brief Gets the Time group from the provided timestamp.
   *
   * @param timestamp The timestamp to calculate the time bucket from.
   * @return TimeBucket The time group from the timestamp.
   */
  static TimeGroup GetTimeGroup(core::Timestamp timestamp) noexcept {
    auto date_time = std::chrono::nanoseconds(timestamp);
    std::chrono::system_clock::time_point converted_time_bucket(date_time);
    auto days_since_epoch = std::chrono::duration_cast<days>(
                                converted_time_bucket.time_since_epoch())
                                .count();
    return days_since_epoch;
  }

  /**
   * @brief Get the Time Groups that the provided timestamps belong to.
   *
   * @param timestamps The timestamps to calculate the time groups from.
   * @return unordered_set<TimeGroup> Time groups
   */
  static std::unordered_set<TimeGroup> GetUniqueTimeGroups(
      const std::vector<core::Timestamp>& timestamps) {
    std::unordered_set<TimeGroup> time_groups;
    for (auto timestamp : timestamps) {
      time_groups.insert(GetTimeGroup(timestamp));
    }
    return time_groups;
  }
};
};  // namespace google::scp::pbs::budget_key_timeframe_manager
