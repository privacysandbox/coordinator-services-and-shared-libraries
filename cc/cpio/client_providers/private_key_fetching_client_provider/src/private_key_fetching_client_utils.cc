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

#include "private_key_fetching_client_utils.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/interface/http_types.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_ENCRYPTION_KEY_TYPE_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_EXPIRATION_TIME_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_INVALID_ENCRYPTION_KEY_TYPE;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_KEY_MATERIAL_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_PUBLIC_KEY_MATERIAL_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_PUBLIC_KEYSET_HANDLE_NOT_FOUND;
using google::scp::core::errors::
    SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_RESOURCE_NAME_NOT_FOUND;
using google::scp::cpio::client_providers::KeyData;
using google::scp::cpio::client_providers::PrivateKeyFetchingResponse;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::vector;

static constexpr char kResourceNameLabel[] = "name";
static constexpr char kEncryptionKeyType[] = "encryptionKeyType";
static constexpr char kMultiPartyEnum[] = "MULTI_PARTY_HYBRID_EVEN_KEYSPLIT";
static constexpr char kSinglePartyEnum[] = "SINGLE_PARTY_HYBRID_KEY";
static constexpr char kPublicKeysetHandle[] = "publicKeysetHandle";
static constexpr char kPublicKeyMaterial[] = "publicKeyMaterial";
static constexpr char kExpirationTime[] = "expirationTime";
static constexpr char kKeyData[] = "keyData";
static constexpr char kPublicKeySignature[] = "publicKeySignature";
static constexpr char kKeyEncryptionKeyUri[] = "keyEncryptionKeyUri";
static constexpr char kKeyMaterial[] = "keyMaterial";

namespace google::scp::cpio::client_providers {

ExecutionResult PrivateKeyFetchingClientUtils::ParsePrivateKey(
    const core::BytesBuffer& body,
    PrivateKeyFetchingResponse& response) noexcept {
  auto json_response =
      nlohmann::json::parse(body.bytes->begin(), body.bytes->end());

  string name;
  auto result = ParseJsonValue(json_response, kResourceNameLabel, name);
  if (!result.Successful()) {
    return FailureExecutionResult(
        SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_RESOURCE_NAME_NOT_FOUND);
  }
  response.resource_name = make_shared<string>(name);

  string handle;
  result = ParseJsonValue(json_response, kPublicKeysetHandle, handle);
  if (!result.Successful()) {
    return FailureExecutionResult(
        SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_PUBLIC_KEYSET_HANDLE_NOT_FOUND);
  }
  response.public_keyset_handle = make_shared<string>(handle);

  string public_key_material;
  result =
      ParseJsonValue(json_response, kPublicKeyMaterial, public_key_material);
  if (!result.Successful()) {
    return FailureExecutionResult(
        SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_PUBLIC_KEY_MATERIAL_NOT_FOUND);
  }
  response.public_key_material = make_shared<string>(public_key_material);

  EncryptionKeyType type;
  result = ParseEncryptionKeyType(json_response, kEncryptionKeyType, type);
  if (!result.Successful()) {
    return result;
  }
  response.encryption_key_type = type;

  uint64_t expired_time;
  result = ParseJsonValue(json_response, kExpirationTime, expired_time);
  if (!result.Successful()) {
    return FailureExecutionResult(
        SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_EXPIRATION_TIME_NOT_FOUND);
  }
  response.expiration_time_ms = expired_time;

  vector<shared_ptr<KeyData>> key_data;
  result = ParseKeyData(json_response, kKeyData, key_data);
  if (!result.Successful()) {
    return result;
  }
  response.key_data =
      vector<shared_ptr<KeyData>>(key_data.begin(), key_data.end());

  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyFetchingClientUtils::ParseEncryptionKeyType(
    const nlohmann::json& json_response, const string& type_tag,
    EncryptionKeyType& key_type) noexcept {
  auto it = json_response.find(type_tag);
  if (it == json_response.end()) {
    return FailureExecutionResult(
        SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_ENCRYPTION_KEY_TYPE_NOT_FOUND);
  }

  if (it.value() == kMultiPartyEnum) {
    key_type = EncryptionKeyType::kMultiPartyHybridEvenKeysplit;
  } else if (it.value() == kSinglePartyEnum) {
    key_type = EncryptionKeyType::kSinglePartyHybridKey;
  } else {
    return FailureExecutionResult(
        SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_INVALID_ENCRYPTION_KEY_TYPE);
  }

  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyFetchingClientUtils::ParseKeyData(
    const nlohmann::json& json_response, const string& key_data_tag,
    vector<shared_ptr<KeyData>>& key_data_list) noexcept {
  auto key_data_json = json_response.find(key_data_tag);
  if (key_data_json == json_response.end()) {
    return FailureExecutionResult(
        SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_KEY_DATA_NOT_FOUND);
  }

  auto key_data_size = key_data_json.value().size();
  auto found_key_material = false;

  for (size_t i = 0; i < key_data_size; ++i) {
    auto json_chunk = key_data_json.value()[i];
    KeyData key_data;

    string kek_uri;
    auto result = ParseJsonValue(json_chunk, kKeyEncryptionKeyUri, kek_uri);
    if (!result.Successful()) {
      return result;
    }
    key_data.key_encryption_key_uri = make_shared<string>(kek_uri);

    string key_material;
    result = ParseJsonValue(json_chunk, kKeyMaterial, key_material);
    if (!result.Successful()) {
      return result;
    }
    key_data.key_material = make_shared<string>(key_material);

    if (!key_material.empty() && !kek_uri.empty()) {
      found_key_material = true;
    }

    string public_key_signature;
    result =
        ParseJsonValue(json_chunk, kPublicKeySignature, public_key_signature);
    if (!result.Successful()) {
      return result;
    }
    key_data.public_key_signature = make_shared<string>(public_key_signature);

    key_data_list.emplace_back(make_shared<KeyData>(key_data));
  }

  // Must have one pair of key_encryption_key_uri and key_material.
  if (!found_key_material) {
    return FailureExecutionResult(
        SC_PRIVATE_KEY_FETCHING_CLIENT_PROVIDER_KEY_MATERIAL_NOT_FOUND);
  }

  return SuccessExecutionResult();
}

}  // namespace google::scp::cpio::client_providers
