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
#include <stdexcept>

#include "cpio/client_providers/global_cpio/mock/mock_lib_cpio_provider.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "cpio/client_providers/interface/cpio_provider_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio {
static std::unique_ptr<client_providers::CpioProviderInterface> cpio_ptr;

static core::ExecutionResult SetGlobalCpio() {
  cpio_ptr = std::make_unique<client_providers::mock::MockLibCpioProvider>();
  auto execution_result = cpio_ptr->Init();
  if (!execution_result.Successful()) {
    return execution_result;
  }
  execution_result = cpio_ptr->Run();
  if (!execution_result.Successful()) {
    return execution_result;
  }
  client_providers::GlobalCpio::SetGlobalCpio(cpio_ptr);

  return core::SuccessExecutionResult();
}

core::ExecutionResult InitCpio() {
  return SetGlobalCpio();
}

core::ExecutionResult ShutdownCpio() {
  if (client_providers::GlobalCpio::GetGlobalCpio()) {
    auto execution_result =
        client_providers::GlobalCpio::GetGlobalCpio()->Stop();
    if (!execution_result.Successful()) {
      return execution_result;
    }
  }

  return core::SuccessExecutionResult();
}
}  // namespace google::scp::cpio
