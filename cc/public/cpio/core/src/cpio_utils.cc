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

#include "cpio_utils.h"

#include <memory>

#include "core/interface/async_executor_interface.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "cpio/client_providers/interface/cpio_provider_interface.h"
#include "public/core/interface/execution_result.h"

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::client_providers::CpioProviderFactory;
using google::scp::cpio::client_providers::CpioProviderInterface;
using google::scp::cpio::client_providers::GlobalCpio;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;

namespace google::scp::cpio {
ExecutionResult CpioUtils::RunAndSetGlobalCpio(
    unique_ptr<CpioProviderInterface> cpio_ptr,
    const shared_ptr<AsyncExecutorInterface>& cpu_async_executor,
    const shared_ptr<AsyncExecutorInterface>& io_async_executor) {
  RETURN_IF_FAILURE(cpio_ptr->Init());
  RETURN_IF_FAILURE(cpio_ptr->Run());
  if (cpu_async_executor) {
    RETURN_IF_FAILURE(cpio_ptr->SetCpuAsyncExecutor(cpu_async_executor));
  }
  if (io_async_executor) {
    RETURN_IF_FAILURE(cpio_ptr->SetIoAsyncExecutor(io_async_executor));
  }
  GlobalCpio::SetGlobalCpio(cpio_ptr);

  return SuccessExecutionResult();
}
}  // namespace google::scp::cpio
