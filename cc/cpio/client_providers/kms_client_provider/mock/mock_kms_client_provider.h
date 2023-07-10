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
#include "cpio/client_providers/interface/kms_client_provider_interface.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers::mock {
class MockKmsClientProvider : public KmsClientProviderInterface {
 public:
  core::ExecutionResult init_result_mock = core::SuccessExecutionResult();

  core::ExecutionResult Init() noexcept override { return init_result_mock; }

  core::ExecutionResult run_result_mock = core::SuccessExecutionResult();

  core::ExecutionResult Run() noexcept override { return run_result_mock; }

  core::ExecutionResult stop_result_mock = core::SuccessExecutionResult();

  core::ExecutionResult Stop() noexcept override { return stop_result_mock; }

  std::function<core::ExecutionResult(
      core::AsyncContext<KmsDecryptRequest, KmsDecryptResponse>&)>
      decrypt_mock;

  core::ExecutionResult decrypt_result_mock = core::SuccessExecutionResult();
  std::shared_ptr<KmsDecryptResponse> decrypt_response_mock;

  core::ExecutionResult Decrypt(
      core::AsyncContext<KmsDecryptRequest, KmsDecryptResponse>&
          decrypt_context) noexcept override {
    if (decrypt_mock) {
      return decrypt_mock(decrypt_context);
    }

    if (decrypt_response_mock) {
      decrypt_context.result = decrypt_result_mock;
      decrypt_context.response = decrypt_response_mock;
      decrypt_context.Finish();
    }
    return decrypt_result_mock;
  }
};
}  // namespace google::scp::cpio::client_providers::mock
