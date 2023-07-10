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
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/logger/mock/mock_log_provider.h"
#include "core/logger/src/logger.h"

namespace google::scp::core::logger::mock {
class MockLogger : public Logger {
 public:
  MockLogger() : Logger(std::make_unique<MockLogProvider>()) {}

  std::vector<std::string> GetMessages() {
    return dynamic_cast<MockLogProvider&>(*log_provider_).messages_;
  }
};
}  // namespace google::scp::core::logger::mock
