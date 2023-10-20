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

#include "public/cpio/interface/cpio.h"

#include <memory>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "core/interface/logger_interface.h"
#include "core/logger/src/log_providers/console_log_provider.h"
#include "core/logger/src/log_providers/syslog/syslog_log_provider.h"
#include "core/logger/src/logger.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "cpio/client_providers/interface/cpio_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"

#include "cpio_utils.h"

using google::scp::core::ExecutionResult;
using google::scp::core::LoggerInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::GlobalLogger;
using google::scp::core::logger::ConsoleLogProvider;
using google::scp::core::logger::Logger;
using google::scp::core::logger::log_providers::SyslogLogProvider;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::LogOption;
using google::scp::cpio::client_providers::CpioProviderFactory;
using google::scp::cpio::client_providers::CpioProviderInterface;
using google::scp::cpio::client_providers::GlobalCpio;
using std::make_shared;
using std::make_unique;
using std::move;
using std::unique_ptr;

namespace google::scp::cpio {
static ExecutionResult SetLogger(const CpioOptions& options) {
  switch (options.log_option) {
    case LogOption::kNoLog:
      break;
    case LogOption::kConsoleLog:
      logger_ptr = make_unique<Logger>(make_unique<ConsoleLogProvider>());
      break;
    case LogOption::kSysLog:
      logger_ptr = make_unique<Logger>(make_unique<SyslogLogProvider>());
      break;
  }
  if (logger_ptr) {
    auto execution_result = logger_ptr->Init();
    if (!execution_result.Successful()) {
      return execution_result;
    }
    execution_result = logger_ptr->Run();
    if (!execution_result.Successful()) {
      return execution_result;
    }
    GlobalLogger::SetGlobalLogger(std::move(logger_ptr));
  }

  return SuccessExecutionResult();
}

static ExecutionResult SetGlobalCpio(const CpioOptions& options) {
  cpio_ptr = CpioProviderFactory::Create(make_shared<CpioOptions>(options));
  CpioUtils::RunAndSetGlobalCpio(move(cpio_ptr), options.cpu_async_executor,
                                 options.io_async_executor);

  return SuccessExecutionResult();
}

ExecutionResult Cpio::InitCpio(CpioOptions options) {
  auto execution_result = SetLogger(options);
  if (!execution_result.Successful()) {
    return execution_result;
  }
#ifdef TEST_CPIO
  return SuccessExecutionResult();
#else
  return SetGlobalCpio(options);
#endif
}

ExecutionResult Cpio::ShutdownCpio(CpioOptions options) {
  if (GlobalLogger::GetGlobalLogger()) {
    auto execution_result = GlobalLogger::GetGlobalLogger()->Stop();
    if (!execution_result.Successful()) {
      return execution_result;
    }
  }
  if (GlobalCpio::GetGlobalCpio()) {
    auto execution_result = GlobalCpio::GetGlobalCpio()->Stop();
    if (!execution_result.Successful()) {
      return execution_result;
    }
  }

  return SuccessExecutionResult();
}

}  // namespace google::scp::cpio
