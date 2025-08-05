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
#include "cc/core/logger/src/log_providers/console_log_provider.h"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "cc/core/common/time_provider/src/time_provider.h"
#include "cc/core/common/uuid/src/uuid.h"

namespace privacy_sandbox::pbs_common {

constexpr size_t nano_seconds_multiplier = (1000 * 1000 * 1000);

ExecutionResult ConsoleLogProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult ConsoleLogProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult ConsoleLogProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

void ConsoleLogProvider::Log(const LogLevel& level, const Uuid& correlation_id,
                             const Uuid& parent_activity_id,
                             const Uuid& activity_id,
                             const std::string_view& component_name,
                             const std::string_view& machine_name,
                             const std::string_view& cluster_name,
                             const std::string_view& location,
                             const std::string_view& message) noexcept {
  auto current_timestamp =
      TimeProvider::GetWallTimestampInNanosecondsAsClockTicks();
  auto current_timestamp_seconds = current_timestamp / nano_seconds_multiplier;
  auto remainder_nano_seconds = (current_timestamp % nano_seconds_multiplier);
  std::stringstream output;
  output << current_timestamp_seconds << "." << remainder_nano_seconds << "|"
         << cluster_name << "|" << machine_name << "|" << component_name << "|"
         << ToString(correlation_id) << "|" << ToString(parent_activity_id)
         << "|" << ToString(activity_id) << "|" << location << "|"
         << static_cast<int>(level) << ": " << message;
  Print(output.str());
}

void ConsoleLogProvider::Print(const std::string& output) noexcept {
  std::cout << output << std::endl;
}
}  // namespace privacy_sandbox::pbs_common
