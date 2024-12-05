/*
 * Copyright 2024 Google LLC
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
#include "stdout_log_provider.h"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/strings/str_split.h"
#include "cc/core/common/uuid/src/uuid.h"

namespace google::scp::core::logger::log_providers {
using ::google::scp::core::common::ToString;
using ::google::scp::core::common::Uuid;
using json = nlohmann::json;

ExecutionResult StdoutLogProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult StdoutLogProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult StdoutLogProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

void StdoutLogProvider::Log(const LogLevel& level, const Uuid& correlation_id,
                            const Uuid& parent_activity_id,
                            const Uuid& activity_id,
                            const std::string_view& component_name,
                            const std::string_view& machine_name,
                            const std::string_view& cluster_name,
                            const std::string_view& location,
                            const std::string_view& message,
                            va_list args) noexcept {
  std::string severity;

  switch (level) {
    case LogLevel::kDebug:
      severity = "DEBUG";
      break;
    case LogLevel::kInfo:
      severity = "INFO";
      break;
    case LogLevel::kWarning:
      severity = "WARNING";
      break;
    case LogLevel::kError:
      severity = "ERROR";
      break;
    case LogLevel::kCritical:
      severity = "CRITICAL";
      break;
    case LogLevel::kAlert:
      severity = "ALERT";
      break;
    case LogLevel::kEmergency:
      severity = "EMERGENCY";
      break;
    case LogLevel::kNone:
      severity = "DEFAULT";
      std::cerr << "Invalid log type";
      break;
  }
  // Cloud Run defines a structured JSON logging pattern such that if certain
  // fields are defined, Cloud Logging will automatically parse them and
  // populate the logs dashboard accordingly. Learn more at:
  // https://cloud.google.com/logging/docs/structured-logging#structured_logging_special_fields
  json logEntry;
  logEntry["severity"] = severity;
  logEntry["message"] = message;
  logEntry["correlation_id"] = ToString(correlation_id);
  // TODO: Propogate trace details from the header and replace
  // parent_activity_id with trace.
  logEntry["parent_activity_id"] = ToString(parent_activity_id);
  logEntry["activity_id"] = ToString(activity_id);
  logEntry["component_name"] = component_name;
  logEntry["machine_name"] = machine_name;
  logEntry["cluster_name"] = cluster_name;

  json sourceLocation;
  std::vector<std::string> locationFields = absl::StrSplit(location, ':');
  sourceLocation["file"] = locationFields.at(0);
  sourceLocation["function"] = locationFields.at(1);
  sourceLocation["line"] = locationFields.at(2);

  logEntry["logging.googleapis.com/sourceLocation"] = sourceLocation;

  std::cout << logEntry.dump() << std::endl;
}
}  // namespace google::scp::core::logger::log_providers
