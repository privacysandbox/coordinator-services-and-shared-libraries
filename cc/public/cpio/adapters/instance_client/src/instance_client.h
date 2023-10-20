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
#include <string>

#include "cpio/client_providers/interface/instance_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/instance_client/instance_client_interface.h"
#include "public/cpio/proto/instance_service/v1/instance_service.pb.h"

namespace google::scp::cpio {
/*! @copydoc InstanceClientInterface
 */
class InstanceClient : public InstanceClientInterface {
 public:
  explicit InstanceClient(const std::shared_ptr<InstanceClientOptions>& options)
      : options_(options) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult GetCurrentInstanceResourceName(
      cmrt::sdk::instance_service::v1::GetCurrentInstanceResourceNameRequest
          request,
      Callback<cmrt::sdk::instance_service::v1::
                   GetCurrentInstanceResourceNameResponse>
          callback) noexcept override;

  core::ExecutionResult GetTagsByResourceName(
      cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest request,
      Callback<cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse>
          callback) noexcept override;

  core::ExecutionResult GetInstanceDetailsByResourceName(
      cmrt::sdk::instance_service::v1::GetInstanceDetailsByResourceNameRequest
          request,
      Callback<cmrt::sdk::instance_service::v1::
                   GetInstanceDetailsByResourceNameResponse>
          callback) noexcept override;

 protected:
  virtual core::ExecutionResult CreateInstanceClientProvider() noexcept;

  std::shared_ptr<client_providers::InstanceClientProviderInterface>
      instance_client_provider_;

 private:
  std::shared_ptr<InstanceClientOptions> options_;
};
}  // namespace google::scp::cpio
