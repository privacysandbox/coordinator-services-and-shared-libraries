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

#include <memory>
#include <string_view>
#include <utility>

#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/logger_interface.h"
#include "cc/core/logger/interface/log_provider_interface.h"
#include "cc/public/core/interface/execution_result.h"

namespace privacy_sandbox::pbs_common {

/*! @copydoc LoggerInterface
 */
class Logger : public LoggerInterface {
 public:
  /// Constructs a new Logger object
  explicit Logger(std::unique_ptr<LogProviderInterface> log_provider)
      : log_provider_(std::move(log_provider)) {}

  google::scp::core::ExecutionResult Init() noexcept override;

  google::scp::core::ExecutionResult Run() noexcept override;

  google::scp::core::ExecutionResult Stop() noexcept override;

  void Info(const std::string_view& component_name,
            const google::scp::core::common::Uuid& correlation_id,
            const google::scp::core::common::Uuid& parent_activity_id,
            const google::scp::core::common::Uuid& activity_id,
            const std::string_view& location, const std::string_view& message,
            ...) noexcept override;

  void Debug(const std::string_view& component_name,
             const google::scp::core::common::Uuid& correlation_id,
             const google::scp::core::common::Uuid& parent_activity_id,
             const google::scp::core::common::Uuid& activity_id,
             const std::string_view& location, const std::string_view& message,
             ...) noexcept override;

  void Warning(const std::string_view& component_name,
               const google::scp::core::common::Uuid& correlation_id,
               const google::scp::core::common::Uuid& parent_activity_id,
               const google::scp::core::common::Uuid& activity_id,
               const std::string_view& location,
               const std::string_view& message, ...) noexcept override;

  void Error(const std::string_view& component_name,
             const google::scp::core::common::Uuid& correlation_id,
             const google::scp::core::common::Uuid& parent_activity_id,
             const google::scp::core::common::Uuid& activity_id,
             const std::string_view& location, const std::string_view& message,
             ...) noexcept override;

  void Alert(const std::string_view& component_name,
             const google::scp::core::common::Uuid& correlation_id,
             const google::scp::core::common::Uuid& parent_activity_id,
             const google::scp::core::common::Uuid& activity_id,
             const std::string_view& location, const std::string_view& message,
             ...) noexcept override;

  void Critical(const std::string_view& component_name,
                const google::scp::core::common::Uuid& correlation_id,
                const google::scp::core::common::Uuid& parent_activity_id,
                const google::scp::core::common::Uuid& activity_id,
                const std::string_view& location,
                const std::string_view& message, ...) noexcept override;

  void Emergency(const std::string_view& component_name,
                 const google::scp::core::common::Uuid& correlation_id,
                 const google::scp::core::common::Uuid& parent_activity_id,
                 const google::scp::core::common::Uuid& activity_id,
                 const std::string_view& location,
                 const std::string_view& message, ...) noexcept override;

 protected:
  /// A unique pointer to the log provider instance.
  std::unique_ptr<LogProviderInterface> log_provider_;
};
}  // namespace privacy_sandbox::pbs_common
