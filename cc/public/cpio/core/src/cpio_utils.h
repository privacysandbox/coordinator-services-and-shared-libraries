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

#include <memory>

#include "core/interface/async_executor_interface.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "cpio/client_providers/interface/cpio_provider_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio {
class CpioUtils {
 public:
  /**
   * @brief Run the given CpioProvider and set it in the GlobalCpio. If passed
   * in external async executors, set them in CpioProvider.
   *
   * @param cpio_ptr the given CpioProvider.
   * @param cpu_async_executor the external CPU AsyncExecutor.
   * @param io_async_executor the external IO AsyncExecutor.
   * @return core::ExecutionResult the operation result.
   */
  static core::ExecutionResult RunAndSetGlobalCpio(
      std::unique_ptr<client_providers::CpioProviderInterface> cpio_ptr,
      const std::shared_ptr<core::AsyncExecutorInterface>& cpu_async_executor,
      const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor);
};
}  // namespace google::scp::cpio
