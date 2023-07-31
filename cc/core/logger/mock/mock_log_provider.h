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
#include <string_view>
#include <vector>

#include "core/common/uuid/src/uuid.h"
#include "core/logger/interface/log_provider_interface.h"
#include "core/logger/src/log_providers/console_log_provider.h"
#include "core/logger/src/log_utils.h"

namespace google::scp::core::logger::mock {
class MockLogProvider : public ConsoleLogProvider {
 public:
  ExecutionResult Init() noexcept { return SuccessExecutionResult(); }

  ExecutionResult Run() noexcept { return SuccessExecutionResult(); }

  ExecutionResult Stop() noexcept { return SuccessExecutionResult(); }

  void Print(const std::string& output) noexcept override {
    messages_.push_back(output);
  }

  std::vector<std::string> messages_;
};
}  // namespace google::scp::core::logger::mock
