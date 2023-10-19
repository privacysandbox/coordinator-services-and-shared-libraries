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

#include "test_lib_cpio.h"

#include <memory>

#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "cpio/client_providers/global_cpio/test/test_lib_cpio_provider.h"
#include "cpio/client_providers/interface/cpio_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/core/src/cpio_utils.h"
#include "public/cpio/interface/cpio.h"
#include "public/cpio/test/global_cpio/test_cpio_options.h"

using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::TestLibCpioProvider;
using std::make_shared;
using std::make_unique;

namespace google::scp::cpio {
static ExecutionResult SetGlobalCpio(const TestCpioOptions& options) {
  cpio_ptr =
      make_unique<TestLibCpioProvider>(make_shared<TestCpioOptions>(options));

  CpioUtils::RunAndSetGlobalCpio(move(cpio_ptr), options.cpu_async_executor,
                                 options.io_async_executor);

  return SuccessExecutionResult();
}

ExecutionResult TestLibCpio::InitCpio(TestCpioOptions options) {
  auto execution_result = Cpio::InitCpio(options.ToCpioOptions());
  if (!execution_result.Successful()) {
    return execution_result;
  }
  return SetGlobalCpio(options);
}

ExecutionResult TestLibCpio::ShutdownCpio(TestCpioOptions options) {
  return Cpio::ShutdownCpio(options.ToCpioOptions());
}
}  // namespace google::scp::cpio
