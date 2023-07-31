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

#include "test_lib_cpio_provider.h"

#include <memory>

#include "cpio/client_providers/global_cpio/src/cpio_provider/lib_cpio_provider.h"
#include "cpio/client_providers/instance_client_provider_new/test/test_instance_client_provider.h"
#include "cpio/client_providers/interface/cpio_provider_interface.h"
#include "public/cpio/interface/type_def.h"

using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::unique_ptr;

namespace google::scp::cpio::client_providers {
TestLibCpioProvider::TestLibCpioProvider(
    const shared_ptr<TestCpioOptions>& test_cpio_options)
    : LibCpioProvider() {
  instance_client_provider_ = make_shared<TestInstanceClientProviderNew>(
      make_shared<TestInstanceClientOptions>(*test_cpio_options));
}

unique_ptr<CpioProviderInterface> CpioProviderFactory::Create(
    const shared_ptr<CpioOptions>& options) {
  return make_unique<TestLibCpioProvider>(
      std::dynamic_pointer_cast<TestCpioOptions>(options));
}
}  // namespace google::scp::cpio::client_providers
