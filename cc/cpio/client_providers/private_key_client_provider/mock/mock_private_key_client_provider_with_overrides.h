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

#include "core/http2_client/mock/mock_http_client.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/kms_client_provider/mock/mock_kms_client_provider.h"
#include "cpio/client_providers/private_key_client_provider/src/private_key_client_provider.h"
#include "cpio/client_providers/private_key_client_provider/src/private_key_client_utils.h"
#include "cpio/client_providers/private_key_fetcher_provider/mock/mock_private_key_fetcher_provider.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers::mock {
class MockPrivateKeyClientProviderWithOverrides
    : public PrivateKeyClientProvider {
 public:
  explicit MockPrivateKeyClientProviderWithOverrides(
      const std::shared_ptr<PrivateKeyClientOptions>&
          private_key_client_options)
      : PrivateKeyClientProvider(
            private_key_client_options,
            std::make_shared<core::http2_client::mock::MockHttpClient>(),
            std::make_shared<MockPrivateKeyFetcherProvider>(),
            std::make_shared<MockKmsClientProvider>()) {}

  std::function<core::ExecutionResult(
      core::AsyncContext<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest,
          cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>&)>
      list_private_keys_by_ids_mock;

  core::ExecutionResult list_private_keys_by_ids_result_mock;

  core::ExecutionResult ListPrivateKeys(
      core::AsyncContext<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest,
          cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>&
          context) noexcept override {
    if (list_private_keys_by_ids_mock) {
      return list_private_keys_by_ids_mock(context);
    }
    if (list_private_keys_by_ids_result_mock) {
      context.result = list_private_keys_by_ids_result_mock;
      if (list_private_keys_by_ids_result_mock ==
          core::SuccessExecutionResult()) {
        context.response = std::make_shared<
            cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>();
      }
      context.Finish();
      return list_private_keys_by_ids_result_mock;
    }

    return PrivateKeyClientProvider::ListPrivateKeys(context);
  }

  std::shared_ptr<MockKmsClientProvider> GetKmsClientProvider() {
    return std::dynamic_pointer_cast<MockKmsClientProvider>(
        kms_client_provider_);
  }

  std::shared_ptr<MockPrivateKeyFetcherProvider>
  GetPrivateKeyFetcherProvider() {
    return std::dynamic_pointer_cast<MockPrivateKeyFetcherProvider>(
        private_key_fetcher_);
  }
};

}  // namespace google::scp::cpio::client_providers::mock
