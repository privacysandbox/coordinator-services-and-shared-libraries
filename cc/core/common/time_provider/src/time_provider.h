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

#include "core/interface/type_def.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::common {
class TimeProvider {
 public:
  /**
   * @brief Get wall-clock (system time) of the system in nanoseconds.
   *
   * @return nanoseconds The timestamp in nanoseconds
   */
  static std::chrono::nanoseconds GetWallTimestampInNanoseconds() {
    return std::chrono::system_clock::now().time_since_epoch();
  }

  /**
   * @brief Get wall-clock (system time) of the system in nanoseconds.
   *
   * @return Timestamp The timestamp in nanoseconds in ticks format
   */
  static Timestamp GetWallTimestampInNanosecondsAsClockTicks() {
    return GetWallTimestampInNanoseconds().count();
  }

  /**
   * @brief Get CPU ticks elapsed since the last reboot in nanoseconds. This
   * is used to measure time intervals.
   *
   * @return nanoseconds The timestamp in nanoseconds
   */
  static std::chrono::nanoseconds GetSteadyTimestampInNanoseconds() {
    return std::chrono::steady_clock::now().time_since_epoch();
  }

  /**
   * @brief Get CPU ticks elapsed since the last reboot in nanoseconds. This
   * is used to measure time intervals.
   *
   * @return Timestamp The timestamp in nanoseconds in ticks format
   */
  static Timestamp GetSteadyTimestampInNanosecondsAsClockTicks() {
    return GetSteadyTimestampInNanoseconds().count();
  }

  /**
   * @brief Get unique wall-clock timestamp of the system. If two or more
   * threads call this function at the same time instant, the function
   * guarantees that they get different timestamps.
   *
   * NOTE: Some details about NTP's clock adjustment. NTP adjusts the clock to a
   * value backwards in time only in few cases when the drift is above a certain
   * threshold, see below man page snippet.
   *
   * $ man ntpd
   *    Under ordinary conditions, ntpd *slews the clock* so that the time is
   *    effectively continuous and never runs backwards. If due to extreme
   *    network congestion an error spike exceeds the step threshold, by default
   *    128 ms, the spike is discarded. However, if the error persists for more
   *    than the stepout threshold, by default 900 s, the system clock is
   *    stepped to the correct value. In practice the need for a step has is
   *    extremely rare and almost always the result of a hardware failure. With
   *    the -x option the step threshold is increased to 600 s.
   * https://www.redhat.com/en/blog/avoiding-clock-drift-vms
   *
   * @return nanoseconds The timestamp in nanoseconds
   */
  static std::chrono::nanoseconds GetUniqueWallTimestampInNanoseconds() {
    static std::atomic<std::chrono::nanoseconds> last_timestamp(
        GetWallTimestampInNanoseconds());
    std::chrono::nanoseconds last_timestamp_read;
    std::chrono::nanoseconds current_timestamp;
    do {
      last_timestamp_read = last_timestamp.load();
      current_timestamp = GetWallTimestampInNanoseconds();
      // If the current wall clock goes backwards in time, give out
      // logical timestamps until the future arrives.
      if (current_timestamp < last_timestamp_read) {
        current_timestamp = last_timestamp_read + std::chrono::nanoseconds(1);
      }
    } while (!last_timestamp.compare_exchange_strong(last_timestamp_read,
                                                     current_timestamp));
    return current_timestamp;
  }
};
};  // namespace google::scp::core::common
