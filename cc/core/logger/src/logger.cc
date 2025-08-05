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

#include "cc/core/logger/src/logger.h"

#include <cstdarg>
#include <memory>
#include <sstream>
#include <string_view>

#include "cc/core/common/uuid/src/uuid.h"
#include "cc/core/interface/logger_interface.h"
#include "cc/core/logger/interface/log_provider_interface.h"
#include "cc/public/core/interface/execution_result.h"

// Default values are empty to save characters on each log on the wire.
static constexpr char kDefaultMachineName[] = "";
static constexpr char kDefaultClusterName[] = "";

namespace privacy_sandbox::pbs_common {

ExecutionResult Logger::Init() noexcept {
  return log_provider_->Init();
}

ExecutionResult Logger::Run() noexcept {
  return log_provider_->Run();
}

ExecutionResult Logger::Stop() noexcept {
  return log_provider_->Stop();
}

void Logger::Info(const std::string_view& component_name,
                  const Uuid& correlation_id, const Uuid& parent_activity_id,
                  const Uuid& activity_id, const std::string_view& location,
                  const std::string_view& message) noexcept {
  log_provider_->Log(LogLevel::kInfo, correlation_id, parent_activity_id,
                     activity_id, component_name, kDefaultMachineName,
                     kDefaultClusterName, location, message);
}

void Logger::Debug(const std::string_view& component_name,
                   const Uuid& correlation_id, const Uuid& parent_activity_id,
                   const Uuid& activity_id, const std::string_view& location,
                   const std::string_view& message) noexcept {
  log_provider_->Log(LogLevel::kDebug, correlation_id, parent_activity_id,
                     activity_id, component_name, kDefaultMachineName,
                     kDefaultClusterName, location, message);
}

void Logger::Warning(const std::string_view& component_name,
                     const Uuid& correlation_id, const Uuid& parent_activity_id,
                     const Uuid& activity_id, const std::string_view& location,
                     const std::string_view& message) noexcept {
  log_provider_->Log(LogLevel::kWarning, correlation_id, parent_activity_id,
                     activity_id, component_name, kDefaultMachineName,
                     kDefaultClusterName, location, message);
}

void Logger::Error(const std::string_view& component_name,
                   const Uuid& correlation_id, const Uuid& parent_activity_id,
                   const Uuid& activity_id, const std::string_view& location,
                   const std::string_view& message) noexcept {
  log_provider_->Log(LogLevel::kError, correlation_id, parent_activity_id,
                     activity_id, component_name, kDefaultMachineName,
                     kDefaultClusterName, location, message);
}

void Logger::Alert(const std::string_view& component_name,
                   const Uuid& correlation_id, const Uuid& parent_activity_id,
                   const Uuid& activity_id, const std::string_view& location,
                   const std::string_view& message) noexcept {
  log_provider_->Log(LogLevel::kAlert, correlation_id, parent_activity_id,
                     activity_id, component_name, kDefaultMachineName,
                     kDefaultClusterName, location, message);
}

void Logger::Critical(const std::string_view& component_name,
                      const Uuid& correlation_id,
                      const Uuid& parent_activity_id, const Uuid& activity_id,
                      const std::string_view& location,
                      const std::string_view& message) noexcept {
  log_provider_->Log(LogLevel::kCritical, correlation_id, parent_activity_id,
                     activity_id, component_name, kDefaultMachineName,
                     kDefaultClusterName, location, message);
}

void Logger::Emergency(const std::string_view& component_name,
                       const Uuid& correlation_id,
                       const Uuid& parent_activity_id, const Uuid& activity_id,
                       const std::string_view& location,
                       const std::string_view& message) noexcept {
  log_provider_->Log(LogLevel::kEmergency, correlation_id, parent_activity_id,
                     activity_id, component_name, kDefaultMachineName,
                     kDefaultClusterName, location, message);
}
}  // namespace privacy_sandbox::pbs_common
