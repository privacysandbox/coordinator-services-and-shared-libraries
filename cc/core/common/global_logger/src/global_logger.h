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
#include <unordered_set>

#include "absl/strings/str_format.h"
#include "cc/core/interface/logger_interface.h"

namespace privacy_sandbox::pbs_common {
class GlobalLogger {
 public:
  static const std::unique_ptr<LoggerInterface>& GetGlobalLogger();
  static bool IsLogLevelEnabled(const LogLevel log_level);
  static void SetGlobalLogLevels(
      const std::unordered_set<LogLevel>& log_levels);
  static void SetGlobalLogger(std::unique_ptr<LoggerInterface> logger);
};
}  // namespace privacy_sandbox::pbs_common

#define SCP_LOCATION                                                        \
  (std::string(__FILE__) + ":" + __func__ + ":" + std::to_string(__LINE__)) \
      .c_str()

#define SCP_INFO(component_name, activity_id, message, ...)                    \
  __SCP_INFO_LOG(component_name, privacy_sandbox::pbs_common::kZeroUuid,       \
                 privacy_sandbox::pbs_common::kZeroUuid, activity_id, message, \
                 ##__VA_ARGS__)

#define SCP_INFO_CONTEXT(component_name, async_context, message, ...)         \
  __SCP_INFO_LOG(component_name, async_context.correlation_id,                \
                 async_context.parent_activity_id, async_context.activity_id, \
                 message, ##__VA_ARGS__)

#define __SCP_INFO_LOG(component_name, correlation_id, parent_activity_id, \
                       activity_id, message, ...)                          \
  if (privacy_sandbox::pbs_common::GlobalLogger::GetGlobalLogger() &&      \
      privacy_sandbox::pbs_common::GlobalLogger::IsLogLevelEnabled(        \
          privacy_sandbox::pbs_common::LogLevel::kInfo)) {                 \
    privacy_sandbox::pbs_common::GlobalLogger::GetGlobalLogger()->Info(    \
        component_name, correlation_id, parent_activity_id, activity_id,   \
        SCP_LOCATION, message, ##__VA_ARGS__);                             \
  }
#define SCP_DEBUG(component_name, activity_id, message, ...)              \
  __SCP_DEBUG_LOG(component_name, privacy_sandbox::pbs_common::kZeroUuid, \
                  privacy_sandbox::pbs_common::kZeroUuid, activity_id,    \
                  message, ##__VA_ARGS__)
#define SCP_DEBUG_CONTEXT(component_name, async_context, message, ...)         \
  __SCP_DEBUG_LOG(component_name, async_context.correlation_id,                \
                  async_context.parent_activity_id, async_context.activity_id, \
                  message, ##__VA_ARGS__)

#define __SCP_DEBUG_LOG(component_name, correlation_id, parent_activity_id, \
                        activity_id, message, ...)                          \
  if (privacy_sandbox::pbs_common::GlobalLogger::GetGlobalLogger() &&       \
      privacy_sandbox::pbs_common::GlobalLogger::IsLogLevelEnabled(         \
          privacy_sandbox::pbs_common::LogLevel::kDebug)) {                 \
    privacy_sandbox::pbs_common::GlobalLogger::GetGlobalLogger()->Debug(    \
        component_name, correlation_id, parent_activity_id, activity_id,    \
        SCP_LOCATION, message, ##__VA_ARGS__);                              \
  }

#define SCP_WARNING(component_name, activity_id, message, ...)              \
  __SCP_WARNING_LOG(component_name, privacy_sandbox::pbs_common::kZeroUuid, \
                    privacy_sandbox::pbs_common::kZeroUuid, activity_id,    \
                    message, ##__VA_ARGS__)

#define SCP_WARNING_CONTEXT(component_name, async_context, message, ...) \
  __SCP_WARNING_LOG(component_name, async_context.correlation_id,        \
                    async_context.parent_activity_id,                    \
                    async_context.activity_id, message, ##__VA_ARGS__)

#define __SCP_WARNING_LOG(component_name, correlation_id, parent_activity_id, \
                          activity_id, message, ...)                          \
  if (privacy_sandbox::pbs_common::GlobalLogger::GetGlobalLogger() &&         \
      privacy_sandbox::pbs_common::GlobalLogger::IsLogLevelEnabled(           \
          privacy_sandbox::pbs_common::LogLevel::kWarning)) {                 \
    privacy_sandbox::pbs_common::GlobalLogger::GetGlobalLogger()->Warning(    \
        component_name, correlation_id, parent_activity_id, activity_id,      \
        SCP_LOCATION, message, ##__VA_ARGS__);                                \
  }

#define SCP_ERROR(component_name, activity_id, execution_result, message, ...) \
  __SCP_ERROR_LOG(component_name, privacy_sandbox::pbs_common::kZeroUuid,      \
                  privacy_sandbox::pbs_common::kZeroUuid, activity_id,         \
                  execution_result, message, ##__VA_ARGS__)

#define SCP_ERROR_CONTEXT(component_name, async_context, execution_result,     \
                          message, ...)                                        \
  __SCP_ERROR_LOG(component_name, async_context.correlation_id,                \
                  async_context.parent_activity_id, async_context.activity_id, \
                  execution_result, message, ##__VA_ARGS__)

#define __SCP_ERROR_LOG(component_name, correlation_id, parent_activity_id, \
                        activity_id, execution_result, message, ...)        \
  if (privacy_sandbox::pbs_common::GlobalLogger::GetGlobalLogger() &&       \
      privacy_sandbox::pbs_common::GlobalLogger::IsLogLevelEnabled(         \
          privacy_sandbox::pbs_common::LogLevel::kError)) {                 \
    auto message_with_error = std::string(message) +                        \
                              std::string(" Failed with: ") +               \
                              privacy_sandbox::pbs_common::GetErrorMessage( \
                                  execution_result.status_code);            \
    privacy_sandbox::pbs_common::GlobalLogger::GetGlobalLogger()->Error(    \
        component_name, correlation_id, parent_activity_id, activity_id,    \
        SCP_LOCATION, message_with_error.c_str(), ##__VA_ARGS__);           \
  }

#define SCP_CRITICAL(component_name, activity_id, execution_result, message, \
                     ...)                                                    \
  __SCP_CRITICAL_LOG(component_name, privacy_sandbox::pbs_common::kZeroUuid, \
                     privacy_sandbox::pbs_common::kZeroUuid, activity_id,    \
                     execution_result, message, ##__VA_ARGS__)

#define SCP_CRITICAL_CONTEXT(component_name, async_context, execution_result, \
                             message, ...)                                    \
  __SCP_CRITICAL_LOG(component_name, async_context.correlation_id,            \
                     async_context.parent_activity_id,                        \
                     async_context.activity_id, execution_result, message,    \
                     ##__VA_ARGS__)

#define __SCP_CRITICAL_LOG(component_name, correlation_id, parent_activity_id, \
                           activity_id, execution_result, message, ...)        \
  if (privacy_sandbox::pbs_common::GlobalLogger::GetGlobalLogger() &&          \
      privacy_sandbox::pbs_common::GlobalLogger::IsLogLevelEnabled(            \
          privacy_sandbox::pbs_common::LogLevel::kCritical)) {                 \
    auto message_with_error = std::string(message) +                           \
                              std::string(" Failed with: ") +                  \
                              privacy_sandbox::pbs_common::GetErrorMessage(    \
                                  execution_result.status_code);               \
    privacy_sandbox::pbs_common::GlobalLogger::GetGlobalLogger()->Critical(    \
        component_name, correlation_id, parent_activity_id, activity_id,       \
        SCP_LOCATION, message_with_error.c_str(), ##__VA_ARGS__);              \
  }

#define SCP_ALERT(component_name, activity_id, execution_result, message, ...) \
  __SCP_ALERT_LOG(component_name, privacy_sandbox::pbs_common::kZeroUuid,      \
                  privacy_sandbox::pbs_common::kZeroUuid, activity_id,         \
                  execution_result, message, ##__VA_ARGS__)

#define SCP_ALERT_CONTEXT(component_name, async_context, execution_result,     \
                          message, ...)                                        \
  __SCP_ALERT_LOG(component_name, async_context.correlation_id,                \
                  async_context.parent_activity_id, async_context.activity_id, \
                  execution_result, message, ##__VA_ARGS__)

#define __SCP_ALERT_LOG(component_name, correlation_id, parent_activity_id, \
                        activity_id, execution_result, message, ...)        \
  if (privacy_sandbox::pbs_common::GlobalLogger::GetGlobalLogger() &&       \
      privacy_sandbox::pbs_common::GlobalLogger::IsLogLevelEnabled(         \
          privacy_sandbox::pbs_common::LogLevel::kAlert)) {                 \
    auto message_with_error = std::string(message) +                        \
                              std::string(" Failed with: ") +               \
                              privacy_sandbox::pbs_common::GetErrorMessage( \
                                  execution_result.status_code);            \
    privacy_sandbox::pbs_common::GlobalLogger::GetGlobalLogger()->Alert(    \
        component_name, correlation_id, parent_activity_id, activity_id,    \
        SCP_LOCATION, message_with_error.c_str(), ##__VA_ARGS__);           \
  }

#define SCP_EMERGENCY(component_name, activity_id, execution_result, message, \
                      ...)                                                    \
  __SCP_EMERGENCY_LOG(component_name, privacy_sandbox::pbs_common::kZeroUuid, \
                      privacy_sandbox::pbs_common::kZeroUuid, activity_id,    \
                      execution_result, message, ##__VA_ARGS__)

#define SCP_EMERGENCY_CONTEXT(component_name, async_context, execution_result, \
                              message, ...)                                    \
  __SCP_EMERGENCY_LOG(component_name, async_context.correlation_id,            \
                      async_context.parent_activity_id,                        \
                      async_context.activity_id, execution_result, message,    \
                      ##__VA_ARGS__)

#define __SCP_EMERGENCY_LOG(component_name, correlation_id,                    \
                            parent_activity_id, activity_id, execution_result, \
                            message, ...)                                      \
  if (privacy_sandbox::pbs_common::GlobalLogger::GetGlobalLogger() &&          \
      privacy_sandbox::pbs_common::GlobalLogger::IsLogLevelEnabled(            \
          privacy_sandbox::pbs_common::LogLevel::kEmergency)) {                \
    auto message_with_error = std::string(message) +                           \
                              std::string(" Failed with: ") +                  \
                              privacy_sandbox::pbs_common::GetErrorMessage(    \
                                  execution_result.status_code);               \
    privacy_sandbox::pbs_common::GlobalLogger::GetGlobalLogger()->Emergency(   \
        component_name, correlation_id, parent_activity_id, activity_id,       \
        SCP_LOCATION, message_with_error.c_str(), ##__VA_ARGS__);              \
  }
