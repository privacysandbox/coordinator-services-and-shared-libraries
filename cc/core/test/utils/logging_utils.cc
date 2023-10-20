// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "logging_utils.h"

#include <memory>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/logger/src/log_providers/console_log_provider.h"
#include "core/logger/src/log_providers/syslog/syslog_log_provider.h"
#include "core/logger/src/logger.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::LoggerInterface;
using google::scp::core::common::GlobalLogger;
using google::scp::core::logger::ConsoleLogProvider;
using google::scp::core::logger::Logger;
using google::scp::core::logger::log_providers::SyslogLogProvider;

namespace google::scp::core::test {

void TestLoggingUtils::EnableLogOutputToConsole() {
  std::unique_ptr<LoggerInterface> logger_ptr =
      std::make_unique<Logger>(std::make_unique<ConsoleLogProvider>());
  EXPECT_SUCCESS(logger_ptr->Init());
  EXPECT_SUCCESS(logger_ptr->Run());
  GlobalLogger::SetGlobalLogger(std::move(logger_ptr));
}

void TestLoggingUtils::EnableLogOutputToSyslog() {
  std::unique_ptr<LoggerInterface> logger_ptr =
      std::make_unique<Logger>(std::make_unique<SyslogLogProvider>());
  EXPECT_SUCCESS(logger_ptr->Init());
  EXPECT_SUCCESS(logger_ptr->Run());
  GlobalLogger::SetGlobalLogger(std::move(logger_ptr));
}

};  // namespace google::scp::core::test
