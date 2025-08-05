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

#include "cc/core/common/global_logger/src/global_logger.h"

#include <memory>
#include <unordered_set>
#include <utility>

namespace privacy_sandbox::pbs_common {
static std::unique_ptr<LoggerInterface> logger_instance_;
static std::unordered_set<LogLevel> enabled_log_levels_ = {
    LogLevel::kAlert,     LogLevel::kCritical, LogLevel::kDebug,
    LogLevel::kEmergency, LogLevel::kError,    LogLevel::kInfo,
    LogLevel::kWarning};

const std::unique_ptr<LoggerInterface>& GlobalLogger::GetGlobalLogger() {
  return logger_instance_;
}

void GlobalLogger::SetGlobalLogLevels(
    const std::unordered_set<LogLevel>& log_levels) {
  enabled_log_levels_ = log_levels;
}

void GlobalLogger::SetGlobalLogger(std::unique_ptr<LoggerInterface> logger) {
  logger_instance_ = std::move(logger);
}

bool GlobalLogger::IsLogLevelEnabled(const LogLevel log_level) {
  return enabled_log_levels_.find(log_level) != enabled_log_levels_.end();
}
}  // namespace privacy_sandbox::pbs_common
