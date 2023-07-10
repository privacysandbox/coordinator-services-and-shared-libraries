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

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Interface responsible to initialize and shutdown cloud.
 */
class CloudInitializerInterface : public core::ServiceInterface {
 public:
  /// Init cloud.
  virtual void InitCloud() noexcept = 0;

  /// Shutdown cloud.
  virtual void ShutdownCloud() noexcept = 0;
};

class CloudInitializerFactory {
 public:
  /**
   * @brief Factory to create CloudInitializer.
   *
   * @return std::shared_ptr<CloudInitializerInterface> created
   * CloudInitializer.
   */
  static std::shared_ptr<CloudInitializerInterface> Create();
};
}  // namespace google::scp::cpio::client_providers
