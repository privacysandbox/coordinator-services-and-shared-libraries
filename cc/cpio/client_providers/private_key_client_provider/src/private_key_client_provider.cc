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
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/private_key_client/type_def.h"
#include "public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

#include "error_codes.h"
#include "private_key_client_utils.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse;
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
    SC_PRIVATE_KEY_CLIENT_PROVIDER_UNMATCHED_ENDPOINTS_SPLITS;
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
ExecutionResult PrivateKeyClientProvider::Init() noexcept {
  endpoint_list_.push_back(
      private_key_client_options_->primary_private_key_vending_endpoint);
  for (const auto& endpoint :
       private_key_client_options_->secondary_private_key_vending_endpoints) {
    endpoint_list_.push_back(endpoint);
  }
  endpoint_count_ = endpoint_list_.size();

  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyClientProvider::ListPrivateKeys(
    AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
        list_private_keys_context) noexcept {
  auto list_keys_status = make_shared<ListPrivateKeysStatus>();
  list_keys_status->listing_method =
      list_private_keys_context.request->key_ids().empty()
          ? ListingMethod::kByMaxAge
          : ListingMethod::kByKeyId;
  if (list_keys_status->listing_method == ListingMethod::kByKeyId) {
    list_keys_status->expected_total_key_count =
        list_private_keys_context.request->key_ids().size();
  }

  list_keys_status->call_count_per_endpoint =
      list_keys_status->listing_method == ListingMethod::kByKeyId
          ? list_keys_status->expected_total_key_count
          : 1;

  for (size_t call_index = 0;
       call_index < list_keys_status->call_count_per_endpoint; ++call_index) {
    auto endpoints_status = make_shared<KeyEndPointsStatus>();

    for (size_t uri_index = 0; uri_index < endpoint_count_; ++uri_index) {
      auto request = make_shared<PrivateKeyFetchingRequest>();

      if (list_keys_status->listing_method == ListingMethod::kByKeyId) {
        request->key_id = make_shared<string>(
            list_private_keys_context.request->key_ids(call_index));
      } else {
        request->max_age_seconds =
            list_private_keys_context.request->max_age_seconds();
      }

      const auto& endpoint = endpoint_list_[uri_index];
      request->key_vending_endpoint =
          make_shared<PrivateKeyVendingEndpoint>(endpoint);

      AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>
          fetch_private_key_context(
              move(request),
              bind(&PrivateKeyClientProvider::OnFetchPrivateKeyCallback, this,
                   list_private_keys_context, _1, list_keys_status,
                   endpoints_status, uri_index),
              list_private_keys_context);

      auto execution_result =
          private_key_fetcher_->FetchPrivateKey(fetch_private_key_context);

      if (!execution_result.Successful()) {
        // To avoid running context.Finish() repeatedly, use
        // compare_exchange_strong() to check if the ListPrivateKeys
        // context has a result.
        auto got_failure = false;
        if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                  true)) {
          list_private_keys_context.result = execution_result;
          list_private_keys_context.Finish();
        }

        SCP_ERROR(kPrivateKeyClientProvider, kZeroUuid, execution_result,
                  "Failed to fetch private key with endpoint %s.",
                  endpoint.private_key_vending_service_endpoint.c_str());
        return execution_result;
      }
    }
  }

  return SuccessExecutionResult();
}

void PrivateKeyClientProvider::OnFetchPrivateKeyCallback(
    AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
        list_private_keys_context,
    AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
        fetch_private_key_context,
    shared_ptr<ListPrivateKeysStatus> list_keys_status,
    shared_ptr<KeyEndPointsStatus> endpoints_status,
    size_t uri_index) noexcept {
  if (list_keys_status->got_failure.load() ||
      list_keys_status->got_empty_key_list.load()) {
    return;
  }

  auto execution_result = fetch_private_key_context.result;
  if (!execution_result.Successful()) {
    auto got_failure = false;
    if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                              true)) {
      list_private_keys_context.result = execution_result;
      list_private_keys_context.Finish();
      SCP_ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                        list_private_keys_context.result,
                        "Failed to fetch private key.");
    }
    return;
  }

  if (list_keys_status->listing_method == ListingMethod::kByMaxAge) {
    list_keys_status->expected_total_key_count =
        fetch_private_key_context.response->encryption_keys.size();
  }

  list_keys_status->total_key_split_count.fetch_add(
      fetch_private_key_context.response->encryption_keys.size());
  list_keys_status->fetching_call_returned_count.fetch_add(1);

  // For empty key list, return immediately.
  if (list_keys_status->expected_total_key_count == 0) {
    auto got_empty_key_list = false;
    if (list_keys_status->got_empty_key_list.compare_exchange_strong(
            got_empty_key_list, true)) {
      list_private_keys_context.result = SuccessExecutionResult();
      list_private_keys_context.response =
          make_shared<ListPrivateKeysResponse>();
      list_private_keys_context.Finish();
      SCP_WARNING_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                          "The private key list is empty.");
    }
    return;
  }

  for (const auto& encryption_key :
       fetch_private_key_context.response->encryption_keys) {
    // Fails the operation if the key data splits size from private key fetch
    // response does not match endpoints number.
    if (encryption_key->key_data.size() != endpoint_count_) {
      if (list_keys_status->listing_method == ListingMethod::kByKeyId) {
        auto got_failure = false;
        if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                  true)) {
          list_private_keys_context.result = FailureExecutionResult(
              SC_PRIVATE_KEY_CLIENT_PROVIDER_UNMATCHED_ENDPOINTS_SPLITS);
          list_private_keys_context.Finish();
          SCP_ERROR_CONTEXT(kPrivateKeyClientProvider,
                            list_private_keys_context,
                            list_private_keys_context.result,
                            "Unmatched endpoints number and private key split "
                            "data size for key ID %s.",
                            encryption_key->key_id->c_str());
        }
        return;
      } else {
        // For ListByAge, the key_data size might not match the endpoint count
        // if the key is corrupted.
        SCP_WARNING_CONTEXT(kPrivateKeyClientProvider,
                            list_private_keys_context,
                            "Unmatched endpoints number and private key split "
                            "data size for key ID %s.",
                            encryption_key->key_id->c_str());
      }
    }

    DecryptRequest kms_decrypt_request;
    execution_result = PrivateKeyClientUtils::GetKmsDecryptRequest(
        encryption_key, kms_decrypt_request);
    if (!execution_result.Successful()) {
      auto got_failure = false;
      if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                true)) {
        list_private_keys_context.result = execution_result;
        list_private_keys_context.Finish();
        SCP_ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                          list_private_keys_context.result,
                          "Failed to get the key data.");
      }
      return;
    }
    kms_decrypt_request.set_account_identity(
        fetch_private_key_context.request->key_vending_endpoint
            ->account_identity);
    kms_decrypt_request.set_kms_region(
        fetch_private_key_context.request->key_vending_endpoint
            ->service_region);
    // Only used for GCP.
    kms_decrypt_request.set_gcp_wip_provider(
        fetch_private_key_context.request->key_vending_endpoint
            ->gcp_wip_provider);
    AsyncContext<DecryptRequest, DecryptResponse> decrypt_context(
        make_shared<DecryptRequest>(kms_decrypt_request),
        bind(&PrivateKeyClientProvider::OnDecrpytCallback, this,
             list_private_keys_context, _1, list_keys_status, endpoints_status,
             encryption_key, uri_index),
        list_private_keys_context);
    execution_result = kms_client_provider_->Decrypt(decrypt_context);

    if (!execution_result.Successful()) {
      auto got_failure = false;
      if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                true)) {
        list_private_keys_context.result = execution_result;
        list_private_keys_context.Finish();
        SCP_ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                          list_private_keys_context.result,
                          "Failed to send decrypt request.");
      }
      return;
    }
  }
}

void PrivateKeyClientProvider::OnDecrpytCallback(
    AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
        list_private_keys_context,
    AsyncContext<DecryptRequest, DecryptResponse>& decrypt_context,
    shared_ptr<ListPrivateKeysStatus> list_keys_status,
    shared_ptr<KeyEndPointsStatus> endpoints_status,
    shared_ptr<EncryptionKey> encryption_key, size_t uri_index) noexcept {
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
      SCP_ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                        list_private_keys_context.result,
                        "Failed to decrypt the encrypt key.");
    }
    return;
  }

  const auto& key_id = *encryption_key->key_id;

  endpoints_status->map_mutex.lock();
  auto it = endpoints_status->plaintext_key_id_map.find(key_id);
  if (it == endpoints_status->plaintext_key_id_map.end()) {
    endpoints_status->plaintext_key_id_map[key_id] =
        std::vector<string>(endpoint_count_);
    endpoints_status->finished_counter_key_id_map[key_id] = 0;
  }

  auto& plaintexts = endpoints_status->plaintext_key_id_map.at(key_id);
  plaintexts.at(uri_index) =
      move(*decrypt_context.response->mutable_plaintext());
  auto endpoint_finished_prev =
      endpoints_status->finished_counter_key_id_map[key_id].fetch_add(1);
  endpoints_status->map_mutex.unlock();

  // Reconstructs the private key after all endpoints operations are complete
  // for the key.
  if (endpoint_finished_prev == plaintexts.size() - 1) {
    PrivateKey private_key;
    execution_result =
        PrivateKeyClientUtils::GetPrivateKeyInfo(encryption_key, private_key);
    if (!execution_result.Successful()) {
      auto got_failure = false;
      if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                true)) {
        list_private_keys_context.result = execution_result;
        list_private_keys_context.Finish();
        SCP_ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                          list_private_keys_context.result,
                          "Failed to get valid private key.");
      }
      return;
    }
    execution_result = PrivateKeyClientUtils::ReconstructXorKeysetHandle(
        plaintexts, *private_key.mutable_private_key());
    if (!execution_result.Successful()) {
      auto got_failure = false;
      if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                true)) {
        list_private_keys_context.result = execution_result;
        list_private_keys_context.Finish();
        SCP_ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                          list_private_keys_context.result,
                          "Failed to concatenate split private keys.");
      }
      return;
    }

    string encoded_key;
    execution_result = Base64Encode(private_key.private_key(), encoded_key);
    if (!execution_result.Successful()) {
      auto got_failure = false;
      if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                true)) {
        list_private_keys_context.result = execution_result;
        list_private_keys_context.Finish();
        SCP_ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                          list_private_keys_context.result,
                          "Failed to encode the private key using base64.");
      }
      return;
    }

    private_key.set_private_key(move(encoded_key));
    list_keys_status->private_key_id_map[key_id] = move(private_key);
  }

  // Finished all remote calls.
  auto finished_key_split_count_prev =
      list_keys_status->finished_key_split_count.fetch_add(1);
  if (list_keys_status->fetching_call_returned_count ==
          list_keys_status->call_count_per_endpoint * endpoint_count_ &&
      finished_key_split_count_prev ==
          list_keys_status->total_key_split_count - 1) {
    list_private_keys_context.response = make_shared<ListPrivateKeysResponse>();
    int count = 0;
    for (auto it = list_keys_status->private_key_id_map.begin();
         it != list_keys_status->private_key_id_map.end(); ++it) {
      *list_private_keys_context.response->add_private_keys() =
          move(it->second);
      ++count;
    }
    list_private_keys_context.result = SuccessExecutionResult();
    list_private_keys_context.Finish();
  }
}

shared_ptr<PrivateKeyClientProviderInterface>
PrivateKeyClientProviderFactory::Create(
    const shared_ptr<PrivateKeyClientOptions>& options,
    const shared_ptr<core::HttpClientInterface>& http_client,
    const shared_ptr<RoleCredentialsProviderInterface>&
        role_credentials_provider,
    const shared_ptr<AuthTokenProviderInterface>& auth_token_provider,
    const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor) {
  auto kms_client_provider = KmsClientProviderFactory::Create(
      make_shared<KmsClientOptions>(), role_credentials_provider,
      io_async_executor);
  auto private_key_fetcher = PrivateKeyFetcherProviderFactory::Create(
      http_client, role_credentials_provider, auth_token_provider);

  return make_shared<PrivateKeyClientProvider>(
      options, http_client, private_key_fetcher, kms_client_provider);
}

}  // namespace google::scp::cpio::client_providers
