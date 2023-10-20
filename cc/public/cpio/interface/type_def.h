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
#include <memory>
#include <string>

#include "core/interface/async_executor_interface.h"
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

/// Option for whether to initialize cloud in Cpio. Customers can configure it
/// in case they need to do initialization and shutdown in their side. In AWS,
/// Cpio will call Aws::InitAPI and Aws::ShutdownAPI. In GCP, no init or
/// shutdown are needed.
enum class CloudInitOption {
  /// Doesn't initialize cloud in CPIO.
  kNoInitInCpio = 1,
  /// Initializes cloud in CPIO.
  kInitInCpio = 2,
};

/// Global options for CPIO.
struct CpioOptions {
  virtual ~CpioOptions() = default;

  /// Default is kNoLog.
  LogOption log_option = LogOption::kNoLog;

  /// Default is kInitInCpio.
  CloudInitOption cloud_init_option = CloudInitOption::kInitInCpio;

  /// Optional CPU thread pool. If not set, an internal thread pool will be
  /// used.
  std::shared_ptr<core::AsyncExecutorInterface> cpu_async_executor;

  /// Optional IO thread pool. If not set, an internal thread pool will be used.
  std::shared_ptr<core::AsyncExecutorInterface> io_async_executor;
};

template <typename TResponse>
using Callback =
    typename std::function<void(const core::ExecutionResult&, TResponse)>;
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_TYPE_DEFS_H_
