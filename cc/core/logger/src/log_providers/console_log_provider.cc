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
#include "console_log_provider.h"

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "core/logger/src/log_utils.h"

using google::scp::core::common::ToString;
using google::scp::core::common::Uuid;
using std::cout;
using std::endl;
using std::string;
using std::string_view;
using std::vector;
using std::vsnprintf;

namespace google::scp::core::logger {

ExecutionResult ConsoleLogProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult ConsoleLogProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult ConsoleLogProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

void ConsoleLogProvider::Log(
    const LogLevel& level, const Uuid& parent_activity_id,
    const Uuid& activity_id, const string_view& component_name,
    const string_view& machine_name, const string_view& cluster_name,
    const string_view& location, const string_view& message,
    va_list args) noexcept {
  std::stringstream output;
  output << ToString(parent_activity_id) + "|" + ToString(activity_id) << "|"
         << component_name << "|" << machine_name << "|" << cluster_name << "|"
         << location << "|" << static_cast<int>(level) << ": ";

  va_list size_args;
  va_copy(size_args, args);
  const auto size = vsnprintf(nullptr, 0U, message.data(), size_args);
  auto output_message = vector<char>(size + 1);
  // vsnprintf adds a terminator at the end, so we need to specify size + 1
  // here.
  vsnprintf(&output_message[0], size + 1, message.data(), args);

  output << string(output_message.data(), size);

  Print(output.str());
}

void ConsoleLogProvider::Print(const string& output) noexcept {
  cout << output << endl;
}
}  // namespace google::scp::core::logger
