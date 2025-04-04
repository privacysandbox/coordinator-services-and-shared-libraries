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
#include "cc/core/logger/src/log_utils.h"

#include <string>

#include "cc/core/interface/logger_interface.h"

namespace privacy_sandbox::pbs_common {

std::string LogLevelToString(const LogLevel& level) {
  switch (level) {
    case LogLevel::kEmergency:
      return "Emergency";
    case LogLevel::kAlert:
      return "Alert";
    case LogLevel::kCritical:
      return "Critical";
    case LogLevel::kDebug:
      return "Debug";
    case LogLevel::kInfo:
      return "Info";
    case LogLevel::kWarning:
      return "Warning";
    case LogLevel::kError:
      return "Error";
    case LogLevel::kNone:
      return "None";
  }
  return "Unknown";
}

LogLevel LogLevelFromString(const std::string& level) {
  if (level == "Emergency") {
    return LogLevel::kEmergency;
  }
  if (level == "Alert") {
    return LogLevel::kAlert;
  }
  if (level == "Critical") {
    return LogLevel::kCritical;
  }
  if (level == "Debug") {
    return LogLevel::kDebug;
  }
  if (level == "Info") {
    return LogLevel::kInfo;
  }
  if (level == "Warning") {
    return LogLevel::kWarning;
  }
  if (level == "Error") {
    return LogLevel::kError;
  }

  return LogLevel::kNone;
}

}  // namespace privacy_sandbox::pbs_common
