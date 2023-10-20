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

#include "core/interface/service_interface.h"
#include "cpio/client_providers/interface/parameter_client_provider_interface.h"

namespace google::scp::cpio {

/**
 * @brief Platform specific factory interface to provide platform specific
 * clients to ParameterService
 *
 */
class ParameterServiceFactoryInterface : public core::ServiceInterface {
 public:
  virtual ~ParameterServiceFactoryInterface() = default;

  /**
   * @brief Creates ParameterClient.
   *
   * @return std::shared_ptr<client_providers::ParameterClientProviderInterface>
   * created ParameterClient.
   */
  virtual std::shared_ptr<client_providers::ParameterClientProviderInterface>
  CreateParameterClient() noexcept = 0;
};
}  // namespace google::scp::cpio
