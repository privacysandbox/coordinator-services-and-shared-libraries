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

#include <functional>
#include <memory>

#include <google/protobuf/util/message_differencer.h>

#include "cpio/client_providers/interface/parameter_client_provider_interface.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

namespace google::scp::cpio::client_providers::mock {
class MockParameterClientProvider : public ParameterClientProviderInterface {
 public:
  core::ExecutionResult Init() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Run() noexcept override {
    return core::SuccessExecutionResult();
  }

  core::ExecutionResult Stop() noexcept override {
    return core::SuccessExecutionResult();
  }

  cmrt::sdk::parameter_service::v1::GetParameterRequest
      get_parameter_request_mock;
  cmrt::sdk::parameter_service::v1::GetParameterResponse
      get_parameter_response_mock;
  core::ExecutionResult get_parameter_result_mock =
      core::SuccessExecutionResult();

  core::ExecutionResult GetParameter(
      core::AsyncContext<
          cmrt::sdk::parameter_service::v1::GetParameterRequest,
          cmrt::sdk::parameter_service::v1::GetParameterResponse>&
          context) noexcept override {
    context.result = get_parameter_result_mock;
    if (google::protobuf::util::MessageDifferencer::Equals(
            get_parameter_request_mock, *context.request)) {
      context.response = std::make_shared<
          cmrt::sdk::parameter_service::v1::GetParameterResponse>(
          get_parameter_response_mock);
    }
    context.Finish();
    return core::SuccessExecutionResult();
  }
};
}  // namespace google::scp::cpio::client_providers::mock
