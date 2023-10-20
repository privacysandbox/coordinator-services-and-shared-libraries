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

#include "cpio/client_providers/interface/cpio_provider_interface.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Holds CpioProvider to provide global Cpio objects.
 *
 */
class GlobalCpio {
 public:
  /**
   * @brief Gets the Global Cpio object.
   *
   * @return const std::unique_ptr<CpioProviderInterface>& the global Cpio
   * object.
   */
  static const std::unique_ptr<CpioProviderInterface>& GetGlobalCpio();

  /**
   * @brief Sets the Global Cpio object.
   *
   * @param cpio sets the global Cpio object.
   */
  static void SetGlobalCpio(std::unique_ptr<CpioProviderInterface>& cpio);
};
}  // namespace google::scp::cpio::client_providers
