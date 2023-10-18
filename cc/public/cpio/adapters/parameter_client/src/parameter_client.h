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

#include "cpio/client_providers/interface/parameter_client_provider_interface.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/parameter_client/parameter_client_interface.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

namespace google::scp::cpio {
/*! @copydoc ParameterClientInterface
 */
class ParameterClient : public ParameterClientInterface {
 public:
  explicit ParameterClient(
      const std::shared_ptr<ParameterClientOptions>& options)
      : options_(options) {}

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult GetParameter(
      cmrt::sdk::parameter_service::v1::GetParameterRequest request,
      Callback<cmrt::sdk::parameter_service::v1::GetParameterResponse>
          callback) noexcept override;

 protected:
  virtual core::ExecutionResult CreateParameterClientProvider() noexcept;
  std::shared_ptr<client_providers::ParameterClientProviderInterface>
      parameter_client_provider_;

 private:
  std::shared_ptr<ParameterClientOptions> options_;
};
}  // namespace google::scp::cpio
