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

#include "core/interface/config_provider_interface.h"
#include "core/interface/service_interface.h"

namespace google::scp::pbs {
/**
 * @brief Wrapper around PBSInstanceMultiPartition to instantiate a
 * platform-specific PBSInstanceMultiPartition
 *
 */
class PBSInstanceMultiPartitionPlatformWrapper : public core::ServiceInterface {
 public:
  PBSInstanceMultiPartitionPlatformWrapper(
      std::shared_ptr<core::ConfigProviderInterface>);

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

 protected:
  std::shared_ptr<core::ServiceInterface> pbs_instance_;
  std::shared_ptr<core::ConfigProviderInterface> config_provider_;
};

}  // namespace google::scp::pbs
