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

#ifndef SCP_CPIO_INTERFACE_TYPE_DEFS_H_
#define SCP_CPIO_INTERFACE_TYPE_DEFS_H_

#include <functional>
#include <string>

#include "public/core/interface/execution_result.h"

namespace google::scp::cpio {
using AccountIdentity = std::string;
using Region = std::string;
using PublicKeyValue = std::string;
using PublicPrivateKeyPairId = std::string;
using Timestamp = int64_t;

/// Option for logging.
enum class LogOption {
  /// Doesn't produce logs in CPIO.
  kNoLog = 1,
  /// Produces logs to console.
  kConsoleLog = 2,
  /// Produces logs to SysLog.
  kSysLog = 3,
};

/// Global options for CPIO.
struct CpioOptions {
  virtual ~CpioOptions() = default;

  /// Default is kNoLog.
  LogOption log_option = LogOption::kNoLog;
};

template <typename TResponse>
using Callback =
    typename std::function<void(const core::ExecutionResult&, TResponse)>;
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_TYPE_DEFS_H_
