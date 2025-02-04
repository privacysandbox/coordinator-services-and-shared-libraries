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
#include <map>
#include <memory>
#include <string>

#include "cc/core/interface/type_def.h"
#include "cc/public/core/interface/execution_result.h"

namespace google::scp::cpio {
/**
 * @brief The structure of TimeEvent which used to record the start time, end
 * time and difference time for one event.
 *
 */
struct TimeEvent {
  /**
   * @brief Construct a new Time Event object. The start_time is the time when
   * object constructed.
   */
  TimeEvent() {
    start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now().time_since_epoch())
                     .count();
  }

  /**
   * @brief When Stop() is called, the end_time is recorded and also the
   * diff_time is the difference between end_time and start_time.
   */
  void Stop() {
    end_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count();
    diff_time = end_time - start_time;
  }

  /// The start time for the event.
  core::Timestamp start_time;
  /// The end time for the event.
  core::Timestamp end_time;
  /// The duration time for the event.
  core::TimeDuration diff_time;
};
}  // namespace google::scp::cpio
