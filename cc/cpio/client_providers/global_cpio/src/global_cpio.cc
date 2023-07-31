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

#include "global_cpio.h"

#include <memory>
#include <utility>

#include "cpio/client_providers/interface/cpio_provider_interface.h"

using std::move;
using std::unique_ptr;

namespace google::scp::cpio::client_providers {
static std::unique_ptr<CpioProviderInterface> cpio_instance_;

const unique_ptr<CpioProviderInterface>& GlobalCpio::GetGlobalCpio() {
  return cpio_instance_;
}

void GlobalCpio::SetGlobalCpio(unique_ptr<CpioProviderInterface>& cpio) {
  cpio_instance_ = move(cpio);
}
}  // namespace google::scp::cpio::client_providers
