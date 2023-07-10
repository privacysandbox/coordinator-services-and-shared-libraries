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

#include "private_key_client_provider.h"

#include <atomic>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "core/interface/async_context.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/http_types.h"
#include "core/utils/src/base64.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "cpio/client_providers/interface/kms_client_provider_interface.h"
#include "cpio/client_providers/interface/private_key_client_provider_interface.h"
#include "cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "cpio/client_providers/interface/type_def.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/private_key_client/type_def.h"
#include "public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

#include "error_codes.h"
#include "private_key_client_utils.h"

using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsRequest;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysByIdsResponse;
using google::cmrt::sdk::private_key_service::v1::PrivateKey;
using google::protobuf::Any;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_PRIVATE_KEY_CLIENT_PROVIDER_UNMATCHED_ENDPOINTS_SPLIT_KEY_DATA;
using google::scp::core::utils::Base64Encode;
using std::atomic;
using std::bind;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::vector;
using std::placeholders::_1;

static constexpr char kPrivateKeyClientProvider[] = "PrivateKeyClientProvider";

namespace google::scp::cpio::client_providers {
PrivateKeyClientProvider::PrivateKeyClientProvider(
    const shared_ptr<PrivateKeyClientOptions>& private_key_client_options,
    const shared_ptr<HttpClientInterface>& http_client,
    const shared_ptr<RoleCredentialsProviderInterface>&
        role_credentials_provider)
    : private_key_client_options_(private_key_client_options) {
  kms_client_provider_ =
      KmsClientProviderFactory::Create(role_credentials_provider);
  private_key_fetching_client_ =
      PrivateKeyFetchingClientProviderFactory::Create(
          http_client, role_credentials_provider);
}

ExecutionResult PrivateKeyClientProvider::Init() noexcept {
  endpoint_list_.push_back(
      private_key_client_options_->primary_private_key_vending_endpoint);
  for (auto endpoint :
       private_key_client_options_->secondary_private_key_vending_endpoints) {
    endpoint_list_.push_back(endpoint);
  }
  endpoint_num_ = endpoint_list_.size();

  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyClientProvider::ListPrivateKeysByIds(
    AsyncContext<ListPrivateKeysByIdsRequest, ListPrivateKeysByIdsResponse>&
        list_private_keys_context) noexcept {
  auto key_ids_num = list_private_keys_context.request->key_ids().size();

  auto list_keys_status = make_shared<ListPrivateKeysStatus>();
  list_keys_status->responses = vector<PrivateKey>(key_ids_num);

  for (size_t key_id_index = 0; key_id_index < key_ids_num; ++key_id_index) {
    auto endpoints_status = make_shared<KeyEndPointsStatus>();
    endpoints_status->responses = vector<string>(endpoint_num_);
    endpoints_status->key_id_index = key_id_index;

    auto key_id = list_private_keys_context.request->key_ids()[key_id_index];
    for (size_t uri_index = 0; uri_index < endpoint_num_; ++uri_index) {
      auto request = make_shared<PrivateKeyFetchingRequest>();
      auto endpoint = endpoint_list_[uri_index];
      request->key_id = make_shared<string>(key_id);
      request->private_key_service_base_uri =
          make_shared<core::Uri>(endpoint.private_key_vending_service_endpoint);
      request->service_region =
          make_shared<std::string>(endpoint.service_region);
      request->account_identity =
          make_shared<AccountIdentity>(endpoint.account_identity);

      AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>
          fetch_private_key_context(
              move(request),
              bind(&PrivateKeyClientProvider::OnFetchPrivateKeyCallback, this,
                   list_private_keys_context, _1, list_keys_status,
                   endpoints_status, uri_index));

      auto execution_result = private_key_fetching_client_->FetchPrivateKey(
          fetch_private_key_context);

      if (!execution_result.Successful()) {
        // To avoid running context.Finish() repeatedly, use
        // compare_exchange_strong() to check if the ListPrivateKeysByIds
        // context has a result.
        auto got_failure = false;
        if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                  true)) {
          list_private_keys_context.result = execution_result;
          list_private_keys_context.Finish();
        }

        ERROR(kPrivateKeyClientProvider, kZeroUuid, kZeroUuid, execution_result,
              "Failed to fetch private key with endpoint %s.",
              endpoint.private_key_vending_service_endpoint.c_str());
        return execution_result;
      }
    }
  }

  return SuccessExecutionResult();
}

void PrivateKeyClientProvider::OnFetchPrivateKeyCallback(
    AsyncContext<ListPrivateKeysByIdsRequest, ListPrivateKeysByIdsResponse>&
        list_private_keys_context,
    AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
        fetch_private_key_context,
    shared_ptr<ListPrivateKeysStatus> list_keys_status,
    shared_ptr<KeyEndPointsStatus> endpoints_status,
    size_t uri_index) noexcept {
  if (list_keys_status->got_failure.load()) {
    return;
  }

  auto execution_result = fetch_private_key_context.result;
  if (!execution_result.Successful()) {
    auto got_failure = false;
    if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                              true)) {
      list_private_keys_context.result = execution_result;
      list_private_keys_context.Finish();
      ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                    list_private_keys_context.result,
                    "Failed to fetch private key.");
    }
    return;
  }

  // Parses the basic information of the private key when the first endpoint is
  // running. Avoid repeatedly parsing the basic information of a key.
  if (uri_index == 0) {
    PrivateKey private_key;
    execution_result = PrivateKeyClientUtils::GetPrivateKeyInfo(
        fetch_private_key_context.response, private_key);
    if (!execution_result.Successful()) {
      auto got_failure = false;
      if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                true)) {
        list_private_keys_context.result = execution_result;
        list_private_keys_context.Finish();
        ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                      list_private_keys_context.result,
                      "Failed to valid private key.");
      }
      return;
    }
    list_keys_status->responses.at(endpoints_status->key_id_index) =
        private_key;

    // Fails the operation if the key data splits size from private key fetch
    // response does not match endpoints number.
    if (fetch_private_key_context.response->key_data.size() !=
        endpoints_status->responses.size()) {
      auto got_failure = false;
      if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                true)) {
        list_private_keys_context.result = FailureExecutionResult(
            SC_PRIVATE_KEY_CLIENT_PROVIDER_UNMATCHED_ENDPOINTS_SPLIT_KEY_DATA);
        list_private_keys_context.Finish();
        ERROR_CONTEXT(
            kPrivateKeyClientProvider, list_private_keys_context,
            list_private_keys_context.result,
            "Unmatched endpoints number and private key split data size.");
      }
      return;
    }
  }

  KmsDecryptRequest kms_decrypt_request;
  execution_result = PrivateKeyClientUtils::GetKmsDecryptRequest(
      fetch_private_key_context.response, kms_decrypt_request);
  if (!execution_result.Successful()) {
    auto got_failure = false;
    if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                              true)) {
      list_private_keys_context.result = execution_result;
      list_private_keys_context.Finish();
      ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                    list_private_keys_context.result,
                    "Failed to get the key data.");
    }
    return;
  }
  kms_decrypt_request.account_identity =
      fetch_private_key_context.request->account_identity;
  kms_decrypt_request.kms_region =
      fetch_private_key_context.request->service_region;
  AsyncContext<KmsDecryptRequest, KmsDecryptResponse> decrypt_context(
      make_shared<KmsDecryptRequest>(kms_decrypt_request),
      bind(&PrivateKeyClientProvider::OnDecrpytCallback, this,
           list_private_keys_context, _1, list_keys_status, endpoints_status,
           uri_index));
  execution_result = kms_client_provider_->Decrypt(decrypt_context);

  if (!execution_result.Successful()) {
    auto got_failure = false;
    if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                              true)) {
      list_private_keys_context.result = execution_result;
      list_private_keys_context.Finish();
      ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                    list_private_keys_context.result,
                    "Failed to send decrypt request.");
    }
    return;
  }
}

void PrivateKeyClientProvider::OnDecrpytCallback(
    AsyncContext<ListPrivateKeysByIdsRequest, ListPrivateKeysByIdsResponse>&
        list_private_keys_context,
    AsyncContext<KmsDecryptRequest, KmsDecryptResponse>& decrypt_context,
    shared_ptr<ListPrivateKeysStatus> list_keys_status,
    shared_ptr<KeyEndPointsStatus> endpoints_status,
    size_t uri_index) noexcept {
  if (list_keys_status->got_failure.load()) {
    return;
  }

  auto execution_result = decrypt_context.result;
  if (!execution_result.Successful()) {
    auto got_failure = false;
    if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                              true)) {
      list_private_keys_context.result = execution_result;
      list_private_keys_context.Finish();
      ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                    list_private_keys_context.result,
                    "Failed to decrypt the encrypt key.");
    }
    return;
  }

  endpoints_status->responses.at(uri_index) =
      *decrypt_context.response->plaintext;
  auto endpoint_finished_prev = endpoints_status->finished_counter.fetch_add(1);

  // Reconstructs the private key after all endpoints operations are complete.
  if (endpoint_finished_prev == endpoints_status->responses.size() - 1) {
    string private_key;
    execution_result = PrivateKeyClientUtils::ReconstructXorKeysetHandle(
        endpoints_status->responses, private_key);

    if (!execution_result.Successful()) {
      auto got_failure = false;
      if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                true)) {
        list_private_keys_context.result = execution_result;
        list_private_keys_context.Finish();
        ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                      list_private_keys_context.result,
                      "Failed to concatenate split private keys.");
      }
      return;
    }

    // Returns a ListPrivateKeysByIds response after all private key operations
    // are complete.
    string encoded_key;
    execution_result = Base64Encode(private_key, encoded_key);
    if (!execution_result.Successful()) {
      auto got_failure = false;
      if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                true)) {
        list_private_keys_context.result = execution_result;
        list_private_keys_context.Finish();
        ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                      list_private_keys_context.result,
                      "Failed to encode the private key using base64.");
      }
      return;
    }

    list_keys_status->responses.at(endpoints_status->key_id_index)
        .set_private_key(encoded_key);
    auto list_keys_finished_prev =
        list_keys_status->finished_counter.fetch_add(1);
    if (list_keys_finished_prev == list_keys_status->responses.size() - 1) {
      list_private_keys_context.response =
          make_shared<ListPrivateKeysByIdsResponse>();
      list_private_keys_context.response->mutable_private_keys()->Add(
          list_keys_status->responses.begin(),
          list_keys_status->responses.end());
      list_private_keys_context.result = SuccessExecutionResult();
      list_private_keys_context.Finish();
    }
  }
}

shared_ptr<PrivateKeyClientProviderInterface>
PrivateKeyClientProviderFactory::Create(
    const std::shared_ptr<PrivateKeyClientOptions>& options,
    const std::shared_ptr<core::HttpClientInterface>& http_client,
    const std::shared_ptr<RoleCredentialsProviderInterface>&
        role_credentials_provider) {
  return make_shared<PrivateKeyClientProvider>(options, http_client,
                                               role_credentials_provider);
}

}  // namespace google::scp::cpio::client_providers
