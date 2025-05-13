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

#include "cc/pbs/interface/type_def.h"

namespace privacy_sandbox::pbs {

class Utils {
  using days = std::chrono::duration<int64_t, std::ratio<86400>>;

 public:
  /**
   * @brief Gets the Time Bucket from the provided timestamp.
   *
   * @param timestamp The timestamp to calculate the time bucket from.
   * @return TimeBucket The time bucket from the timestamp.
   */
  static TimeBucket GetTimeBucket(pbs_common::Timestamp timestamp) noexcept {
    auto days_since_epoch = GetTimeGroup(timestamp);
    std::chrono::nanoseconds bucket_time_nano_seconds(timestamp);
    std::chrono::nanoseconds days_since_epoch_nano_seconds =
        days(days_since_epoch);

    return std::chrono::duration_cast<std::chrono::hours>(
               bucket_time_nano_seconds - days_since_epoch_nano_seconds)
        .count();
  }

  /**
   * @brief Gets the Time group from the provided timestamp.
   *
   * @param timestamp The timestamp to calculate the time bucket from.
   * @return TimeBucket The time group from the timestamp.
   */
  static TimeGroup GetTimeGroup(pbs_common::Timestamp timestamp) noexcept {
    auto date_time = std::chrono::nanoseconds(timestamp);
    std::chrono::system_clock::time_point converted_time_bucket(date_time);
    auto days_since_epoch = std::chrono::duration_cast<days>(
                                converted_time_bucket.time_since_epoch())
                                .count();
    return days_since_epoch;
  }
};
};  // namespace privacy_sandbox::pbs
