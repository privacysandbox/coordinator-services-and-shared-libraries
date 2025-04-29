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

#include <string>
#include <string_view>

#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/logger/interface/log_provider_interface.h"

namespace privacy_sandbox::pbs_common {
/**
 * @brief Logs messages to the console.
 *
 */
class ConsoleLogProvider : public LogProviderInterface {
 public:
  google::scp::core::ExecutionResult Init() noexcept override;

  google::scp::core::ExecutionResult Run() noexcept override;

  google::scp::core::ExecutionResult Stop() noexcept override;

  void Log(const LogLevel& level,
           const privacy_sandbox::pbs_common::Uuid& correlation_id,
           const privacy_sandbox::pbs_common::Uuid& parent_activity_id,
           const privacy_sandbox::pbs_common::Uuid& activity_id,
           const std::string_view& component_name,
           const std::string_view& machine_name,
           const std::string_view& cluster_name,
           const std::string_view& location, const std::string_view& message,
           va_list args) noexcept override;

 protected:
  virtual void Print(const std::string& output) noexcept;
};
}  // namespace privacy_sandbox::pbs_common
