/*
 * Copyright 2023 Google LLC
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
class Stopwatch {
 public:
  /**
   * @brief Start the stop watch.
   * This captures the time at the moment this function is called.
   */
  virtual void Start();

  /**
   * @brief Stop the stop watch.
   * This captures the time at the moment this function is called and returns
   * the difference between the "now" time and what was captured when Start was
   * called.
   *
   * @return std::chrono::nanoseconds
   */
  virtual std::chrono::nanoseconds Stop();

 private:
  std::chrono::nanoseconds start_{0};
};
}  // namespace google::scp::core::common
