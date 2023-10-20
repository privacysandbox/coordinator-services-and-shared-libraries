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

#include <string_view>

#include "core/common/uuid/src/uuid.h"

#include "service_interface.h"
#include "type_def.h"

namespace google::scp::core {
/// The log level
enum class LogLevel {
  kEmergency = 0,
  kAlert = 1,
  kCritical = 2,
  kError = 4,
  kWarning = 8,
  kDebug = 16,
  kInfo = 32,
  kNone = 64
};

/**
 * @brief The LoggerInterface is used to write to the application log.
 *
 */
class LoggerInterface : public ServiceInterface {
 public:
  virtual ~LoggerInterface() = default;

  /**
   * @brief Logs a message with Info severity.
   *
   * @param component_name The name of the component writing the log.
   * @param parent_activity_id The parent activity id of the log.
   * @param activity_id The activity id of the log.
   * @param correlation_id The correlation id of the log.
   * @param location The file, function, and line where the log was triggered.
   * @param message The actual message to log.
   */
  virtual void Info(const std::string_view& component_name,
                    const common::Uuid& parent_activity_id,
                    const common::Uuid& activity_id,
                    const common::Uuid& correlation_id,
                    const std::string_view& location,
                    const std::string_view& message, ...) noexcept = 0;

  /**
   * @brief Logs a message with Debug severity.
   *
   * @param component_name The name of the component writing the log.
   * @param parent_activity_id The parent activity id of the log.
   * @param activity_id The activity id of the log.
   * @param correlation_id The correlation id of the log.
   * @param location The file, function, and line where the log was triggered.
   * @param message The actual message to log.
   */
  virtual void Debug(const std::string_view& component_name,
                     const common::Uuid& parent_activity_id,
                     const common::Uuid& activity_id,
                     const common::Uuid& correlation_id,
                     const std::string_view& location,
                     const std::string_view& message, ...) noexcept = 0;

  /**
   * @brief Logs a message with Warning severity.
   *
   * @param component_name The name of the component writing the log.
   * @param parent_activity_id The parent activity id of the log.
   * @param activity_id The activity id of the log.
   * @param correlation_id The correlation id of the log.
   * @param location The file, function, and line where the log was triggered.
   * @param message The actual message to log.
   */
  virtual void Warning(const std::string_view& component_name,
                       const common::Uuid& parent_activity_id,
                       const common::Uuid& activity_id,
                       const common::Uuid& correlation_id,
                       const std::string_view& location,
                       const std::string_view& message, ...) noexcept = 0;

  /**
   * @brief Logs a message with Error severity.
   *
   * @param component_name The name of the component writing the log.
   * @param parent_activity_id The parent activity id of the log.
   * @param activity_id The activity id of the log.
   * @param correlation_id The correlation id of the log.
   * @param location The file, function, and line where the log was triggered.
   * @param message The actual message to log.
   */
  virtual void Error(const std::string_view& component_name,
                     const common::Uuid& parent_activity_id,
                     const common::Uuid& activity_id,
                     const common::Uuid& correlation_id,
                     const std::string_view& location,
                     const std::string_view& message, ...) noexcept = 0;

  /**
   * @brief Logs a message with Alert severity.
   *
   * @param component_name The name of the component writing the log.
   * @param parent_activity_id The parent activity id of the log.
   * @param activity_id The activity id of the log.
   * @param correlation_id The correlation id of the log.
   * @param location The file, function, and line where the log was triggered.
   * @param message The actual message to log.
   */
  virtual void Alert(const std::string_view& component_name,
                     const common::Uuid& parent_activity_id,
                     const common::Uuid& activity_id,
                     const common::Uuid& correlation_id,
                     const std::string_view& location,
                     const std::string_view& message, ...) noexcept = 0;

  /**
   * @brief Logs a message with Critical severity.
   *
   * @param component_name The name of the component writing the log.
   * @param parent_activity_id The parent activity id of the log.
   * @param activity_id The activity id of the log.
   * @param correlation_id The correlation id of the log.
   * @param location The file, function, and line where the log was triggered.
   * @param message The actual message to log.
   */
  virtual void Critical(const std::string_view& component_name,
                        const common::Uuid& parent_activity_id,
                        const common::Uuid& activity_id,
                        const common::Uuid& correlation_id,
                        const std::string_view& location,
                        const std::string_view& message, ...) noexcept = 0;

  /**
   * @brief Logs a message with Emergency severity.
   *
   * @param component_name The name of the component writing the log.
   * @param parent_activity_id The parent activity id of the log.
   * @param activity_id The activity id of the log.
   * @param correlation_id The correlation id of the log.
   * @param location The file, function, and line where the log was triggered.
   * @param message The actual message to log.
   */
  virtual void Emergency(const std::string_view& component_name,
                         const common::Uuid& parent_activity_id,
                         const common::Uuid& activity_id,
                         const common::Uuid& correlation_id,
                         const std::string_view& location,
                         const std::string_view& message, ...) noexcept = 0;
};
}  // namespace google::scp::core
