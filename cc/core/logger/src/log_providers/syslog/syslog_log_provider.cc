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

#include "cc/core/logger/src/log_providers/syslog/syslog_log_provider.h"

#include <stdio.h>
#include <syslog.h>

#include <csignal>
#include <cstdarg>
#include <iostream>
#include <string>
#include <string_view>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/logger/src/log_providers/syslog/error_codes.h"
#include "cc/core/logger/src/log_utils.h"

namespace privacy_sandbox::pbs_common {

using ::absl::StrCat;
using ::std::cerr;
using ::std::string;
using ::std::string_view;

ExecutionResult SyslogLogProvider::Init() noexcept {
  try {
    openlog(log_channel, LOG_CONS | LOG_NDELAY, LOG_USER);
  } catch (...) {
    return FailureExecutionResult(SC_SYSLOG_OPEN_CONNECTION_ERROR);
  }
  return SuccessExecutionResult();
}

ExecutionResult SyslogLogProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult SyslogLogProvider::Stop() noexcept {
  try {
    closelog();
  } catch (...) {
    return FailureExecutionResult(SC_SYSLOG_CLOSE_CONNECTION_ERROR);
  }

  return SuccessExecutionResult();
}

void SyslogLogProvider::Log(const LogLevel& level, const Uuid& correlation_id,
                            const Uuid& parent_activity_id,
                            const Uuid& activity_id,
                            const string_view& component_name,
                            const string_view& machine_name,
                            const string_view& cluster_name,
                            const string_view& location,
                            const string_view& message) noexcept {
  std::string severity = absl::AsciiStrToUpper(LogLevelToString(level));

  auto formatted_message = StrCat(
      severity, "|", cluster_name, "|", machine_name, "|", component_name, "|",
      ToString(correlation_id), "|", ToString(parent_activity_id), "|",
      ToString(activity_id), "|", location, "|", message);

  try {
    switch (level) {
      case LogLevel::kDebug:
        syslog(LOG_DEBUG, "%s", formatted_message.c_str());
        break;
      case LogLevel::kInfo:
        syslog(LOG_INFO, "%s", formatted_message.c_str());
        break;
      case LogLevel::kWarning:
        syslog(LOG_WARNING, "%s", formatted_message.c_str());
        break;
      case LogLevel::kError:
        syslog(LOG_ERR, "%s", formatted_message.c_str());
        break;
      case LogLevel::kAlert:
        syslog(LOG_ALERT, "%s", formatted_message.c_str());
        break;
      case LogLevel::kEmergency:
        syslog(LOG_EMERG, "%s", formatted_message.c_str());
        break;
      case LogLevel::kCritical:
        syslog(LOG_CRIT, "%s", formatted_message.c_str());
        break;
      case LogLevel::kNone:
        cerr << "Invalid log type";
        break;
    }
  } catch (...) {
    // TODO: Add code to get exception message
    cerr << "Exception thrown while writing to syslog";
  }
}
}  // namespace privacy_sandbox::pbs_common
