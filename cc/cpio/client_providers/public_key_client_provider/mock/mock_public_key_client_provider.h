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
#include <vector>

#include <google/protobuf/util/message_differencer.h>

#include "core/interface/async_context.h"
#include "core/message_router/src/message_router.h"
#include "cpio/client_providers/interface/public_key_client_provider_interface.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers::mock {
class MockPublicKeyClientProvider : public PublicKeyClientProviderInterface {
 public:
  core::ExecutionResult init_result_mock = core::SuccessExecutionResult();

  core::ExecutionResult Init() noexcept override { return init_result_mock; }

  core::ExecutionResult run_result_mock = core::SuccessExecutionResult();

  core::ExecutionResult Run() noexcept override { return run_result_mock; }

  core::ExecutionResult stop_result_mock = core::SuccessExecutionResult();

  core::ExecutionResult Stop() noexcept override { return stop_result_mock; }

  std::function<core::ExecutionResult(
      core::AsyncContext<
          cmrt::sdk::public_key_service::v1::ListPublicKeysRequest,
          cmrt::sdk::public_key_service::v1::ListPublicKeysResponse>&)>
      list_public_keys_mock;

  core::ExecutionResult list_public_keys_result_mock;
  cmrt::sdk::public_key_service::v1::ListPublicKeysRequest
      list_public_keys_request_mock;
  cmrt::sdk::public_key_service::v1::ListPublicKeysResponse
      list_public_keys_response_mock;

  core::ExecutionResult ListPublicKeys(
      core::AsyncContext<
          cmrt::sdk::public_key_service::v1::ListPublicKeysRequest,
          cmrt::sdk::public_key_service::v1::ListPublicKeysResponse>&
          context) noexcept override {
    if (list_public_keys_mock) {
      return list_public_keys_mock(context);
    }
    google::protobuf::util::MessageDifferencer differencer;
    differencer.set_repeated_field_comparison(
        google::protobuf::util::MessageDifferencer::AS_SET);
    if (differencer.Equals(list_public_keys_request_mock, *context.request)) {
      context.result = list_public_keys_result_mock;
      if (list_public_keys_result_mock == core::SuccessExecutionResult()) {
        context.response = std::make_shared<
            cmrt::sdk::public_key_service::v1::ListPublicKeysResponse>(
            list_public_keys_response_mock);
      }
      context.Finish();
    }
    return list_public_keys_result_mock;
  }
};
}  // namespace google::scp::cpio::client_providers::mock
