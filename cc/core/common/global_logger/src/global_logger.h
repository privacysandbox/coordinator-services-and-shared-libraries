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

#include <cstdarg>
#include <memory>
#include <string>
#include <unordered_set>

#include "core/interface/errors.h"
#include "core/interface/logger_interface.h"

namespace google::scp::core::common {
class GlobalLogger {
 public:
  static const std::unique_ptr<core::LoggerInterface>& GetGlobalLogger();
  static bool IsLogLevelEnabled(const LogLevel log_level);
  static void SetGlobalLogLevels(
      const std::unordered_set<LogLevel>& log_levels);
  static void SetGlobalLogger(std::unique_ptr<core::LoggerInterface>& logger);
};
}  // namespace google::scp::core::common

#define LOCATION                                                            \
  (std::string(__FILE__) + ":" + __func__ + ":" + std::to_string(__LINE__)) \
      .c_str()

#define INFO(component_name, parent_activity_id, activity_id, message, ...) \
  if (google::scp::core::common::GlobalLogger::GetGlobalLogger() &&         \
      google::scp::core::common::GlobalLogger::IsLogLevelEnabled(           \
          google::scp::core::LogLevel::kInfo)) {                            \
    google::scp::core::common::GlobalLogger::GetGlobalLogger()->Info(       \
        component_name, parent_activity_id, activity_id, LOCATION, message, \
        ##__VA_ARGS__);                                                     \
  }

#define INFO_CONTEXT(component_name, async_context, message, ...) \
  INFO(component_name, async_context.parent_activity_id,          \
       async_context.activity_id, message, ##__VA_ARGS__)

#define DEBUG(component_name, parent_activity_id, activity_id, message, ...) \
  if (google::scp::core::common::GlobalLogger::GetGlobalLogger() &&          \
      google::scp::core::common::GlobalLogger::IsLogLevelEnabled(            \
          google::scp::core::LogLevel::kDebug)) {                            \
    google::scp::core::common::GlobalLogger::GetGlobalLogger()->Debug(       \
        component_name, parent_activity_id, activity_id, LOCATION, message,  \
        ##__VA_ARGS__);                                                      \
  }

#define DEBUG_CONTEXT(component_name, async_context, message, ...) \
  DEBUG(component_name, async_context.parent_activity_id,          \
        async_context.activity_id, message, ##__VA_ARGS__)

#define WARNING(component_name, parent_activity_id, activity_id, message, ...) \
  if (google::scp::core::common::GlobalLogger::GetGlobalLogger() &&            \
      google::scp::core::common::GlobalLogger::IsLogLevelEnabled(              \
          google::scp::core::LogLevel::kWarning)) {                            \
    google::scp::core::common::GlobalLogger::GetGlobalLogger()->Warning(       \
        component_name, parent_activity_id, activity_id, LOCATION, message,    \
        ##__VA_ARGS__);                                                        \
  }

#define WARNING_CONTEXT(component_name, async_context, message, ...) \
  WARNING(component_name, async_context.parent_activity_id,          \
          async_context.activity_id, message, ##__VA_ARGS__)

#define ERROR(component_name, parent_activity_id, activity_id,            \
              execution_result, message, ...)                             \
  if (google::scp::core::common::GlobalLogger::GetGlobalLogger() &&       \
      google::scp::core::common::GlobalLogger::IsLogLevelEnabled(         \
          google::scp::core::LogLevel::kError)) {                         \
    auto message_with_error = std::string(message) +                      \
                              std::string(" Failed with: ") +             \
                              google::scp::core::errors::GetErrorMessage( \
                                  execution_result.status_code);          \
    google::scp::core::common::GlobalLogger::GetGlobalLogger()->Error(    \
        component_name, parent_activity_id, activity_id, LOCATION,        \
        message_with_error.c_str(), ##__VA_ARGS__);                       \
  }

#define ERROR_CONTEXT(component_name, async_context, execution_result, \
                      message, ...)                                    \
  ERROR(component_name, async_context.parent_activity_id,              \
        async_context.activity_id, execution_result, message, ##__VA_ARGS__)

#define CRITICAL(component_name, parent_activity_id, activity_id,         \
                 execution_result, message, ...)                          \
  if (google::scp::core::common::GlobalLogger::GetGlobalLogger() &&       \
      google::scp::core::common::GlobalLogger::IsLogLevelEnabled(         \
          google::scp::core::LogLevel::kCritical)) {                      \
    auto message_with_error = std::string(message) +                      \
                              std::string(" Failed with: ") +             \
                              google::scp::core::errors::GetErrorMessage( \
                                  execution_result.status_code);          \
    google::scp::core::common::GlobalLogger::GetGlobalLogger()->Critical( \
        component_name, parent_activity_id, activity_id, LOCATION,        \
        message_with_error.c_str(), ##__VA_ARGS__);                       \
  }

#define CRITICAL_CONTEXT(component_name, async_context, execution_result, \
                         message, ...)                                    \
  CRITICAL(component_name, async_context.parent_activity_id,              \
           async_context.activity_id, execution_result, message,          \
           ##__VA_ARGS__)

#define ALERT(component_name, parent_activity_id, activity_id,            \
              execution_result, message, ...)                             \
  if (google::scp::core::common::GlobalLogger::GetGlobalLogger() &&       \
      google::scp::core::common::GlobalLogger::IsLogLevelEnabled(         \
          google::scp::core::LogLevel::kAlert)) {                         \
    auto message_with_error = std::string(message) +                      \
                              std::string(" Failed with: ") +             \
                              google::scp::core::errors::GetErrorMessage( \
                                  execution_result.status_code);          \
    google::scp::core::common::GlobalLogger::GetGlobalLogger()->Alert(    \
        component_name, parent_activity_id, activity_id, LOCATION,        \
        message_with_error.c_str(), ##__VA_ARGS__);                       \
  }

#define ALERT_CONTEXT(component_name, async_context, execution_result, \
                      message, ...)                                    \
  ALERT(component_name, async_context.parent_activity_id,              \
        async_context.activity_id, execution_result, message, ##__VA_ARGS__)

#define EMERGENCY(component_name, parent_activity_id, activity_id,         \
                  execution_result, message, ...)                          \
  if (google::scp::core::common::GlobalLogger::GetGlobalLogger() &&        \
      google::scp::core::common::GlobalLogger::IsLogLevelEnabled(          \
          google::scp::core::LogLevel::kEmergency)) {                      \
    auto message_with_error = std::string(message) +                       \
                              std::string(" Failed with: ") +              \
                              google::scp::core::errors::GetErrorMessage(  \
                                  execution_result.status_code);           \
    google::scp::core::common::GlobalLogger::GetGlobalLogger()->Emergency( \
        component_name, parent_activity_id, activity_id, LOCATION,         \
        message_with_error.c_str(), ##__VA_ARGS__);                        \
  }

#define EMERGENCY_CONTEXT(component_name, async_context, execution_result, \
                          message, ...)                                    \
  EMERGENCY(component_name, async_context.parent_activity_id,              \
            async_context.activity_id, execution_result, message,          \
            ##__VA_ARGS__)
