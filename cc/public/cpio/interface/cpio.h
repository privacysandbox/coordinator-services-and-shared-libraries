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
#ifndef SCP_CPIO_INTERFACE_CPIO_H_
#define SCP_CPIO_INTERFACE_CPIO_H_

#include <memory>

#include "core/interface/logger_interface.h"
#include "cpio/client_providers/interface/cpio_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/type_def.h"

namespace google::scp::cpio {
static std::unique_ptr<core::LoggerInterface> logger_ptr;
static std::unique_ptr<client_providers::CpioProviderInterface> cpio_ptr;

/**
 * @brief To initialize and shutdown global CPIO objects.
 *
 * Applications that use CPIO should initialize it first, and CPIO should be
 * shut down before the application terminates. CPIO doesn't hold any external
 * resources currently. The components' start/setup and stop/shutdown should be
 * in a reverse order. InitCpio should be called at first and ShutdownCpio
 * should be called at last.
 *
 */
class Cpio {
 public:
  /**
   * @brief Initializes global CPIO objects.
   *
   * @param options global configurations.
   * @return core::ExecutionResult result of initializing CPIO.
   */
  static core::ExecutionResult InitCpio(CpioOptions options);

  /**
   * @brief Shuts down global CPIO objects.
   *
   * @param options global configurations.
   * @return core::ExecutionResult result of terminating CPIO.
   */
  static core::ExecutionResult ShutdownCpio(CpioOptions options);
};
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_CPIO_H_
