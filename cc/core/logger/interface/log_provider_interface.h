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
#include <string_view>

#include "cc/core/interface/logger_interface.h"
#include "cc/core/interface/service_interface.h"
#include "core/common/uuid/src/uuid.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::logger {
/**
 * @brief The LogProviderInterface is implemented by classes that log messages
 * to a specific destination.
 *
 */
class LogProviderInterface : public ServiceInterface {
 public:
  virtual ~LogProviderInterface() = default;

  /**
   * @brief Logs a formatted message to the underlying provider.
   *
   * @param level The severity level of the message.
   * @param parent_activity_id The activity id associated with the message.
   * @param activity_id The activity id associated with the message.
   * @param correlation_id The correlation id associated with the message.
   * @param component_name The name of the component logging the message.
   * @param machine_name The name of the machine logging the message.
   * @param cluster_name The name of the machine cluster logging the
   * message.
   * @param location The file, function, and line where the log was
   * triggered.
   * @param message The message that gets logged.
   * @param ... A set of strings to be formatted into the message.
   */
  virtual void Log(const LogLevel& level,
                   const common::Uuid& parent_activity_id,
                   const common::Uuid& activity_id,
                   const common::Uuid& correlation_id,
                   const std::string_view& component_name,
                   const std::string_view& machine_name,
                   const std::string_view& cluster_name,
                   const std::string_view& location,
                   const std::string_view& message, va_list args) noexcept = 0;
};
}  // namespace google::scp::core::logger
