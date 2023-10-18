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
#include <vector>

#include <nlohmann/json.hpp>

#include "core/interface/http_types.h"
#include "cpio/client_providers/interface/private_key_fetcher_provider_interface.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
class PrivateKeyFetchingClientUtils {
 public:
  /**
   * @brief Extract key ID from resource name
   *
   * @param resource_name the resource name.
   * @return core::ExecutionResultOr<string> the extract result.
   */
  static core::ExecutionResultOr<std::string> ExtractKeyId(
      const std::string& resource_name) noexcept;

  /**
   * @brief Parse PrivateKey from BytesBuffer.
   *
   * @param[in] body BytesBuffer body from http response.
   * @param[out] response PrivateKeyFetchingResponse response object.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult ParsePrivateKey(
      const core::BytesBuffer& body,
      PrivateKeyFetchingResponse& response) noexcept;

  /**
   * @brief Create a Http Request object to query private key vending endpoint.
   *
   * @param private_key_fetching_request request to query private key.
   * @param http_request returned http request.
   */
  static void CreateHttpRequest(
      const PrivateKeyFetchingRequest& private_key_fetching_request,
      core::HttpRequest& http_request);

 protected:
  /**
   * @brief Parse json basic_string value.
   *
   * @tparam T
   * @param json_response json object.
   * @param json_tag json tag.
   * @param[out] json_value the value of the json tag.
   * @return core::ExecutionResult
   */
  template <typename T>
  static core::ExecutionResult ParseJsonValue(
      const nlohmann::json& json_response, const std::string& json_tag,
      T& json_value) noexcept {
    auto it = json_response.find(json_tag);
    if (it == json_response.end()) {
      return core::FailureExecutionResult(
          core::errors::SC_PRIVATE_KEY_FETCHER_PROVIDER_JSON_TAG_NOT_FOUND);
    }
    json_value = it.value();
    return core::SuccessExecutionResult();
  }

  /**
   * @brief Parse EncryptionKey from json.
   *
   * @param json_key json object.
   * @param[out] response response with parsed encryption key.
   * @return core::ExecutionResult parse result.
   */
  static core::ExecutionResult ParseEncryptionKey(
      const nlohmann::json& json_key,
      PrivateKeyFetchingResponse& response) noexcept;
  /**
   * @brief Parse EncryptionKeyType type from json response.
   *
   * @param json_response json object.
   * @param type_tag json tag.
   * @param[out] key_type EncryptionKeyType type.
   * @return core::ExecutionResult parse result.
   */
  static core::ExecutionResult ParseEncryptionKeyType(
      const nlohmann::json& json_response, const std::string& type_tag,
      EncryptionKeyType& key_type) noexcept;

  /**
   * @brief Parse KeyData from json response.
   *
   * @param json_response json object.
   * @param key_data_tag key_data json tag.
   * @param[out] key_data list of KeyData output object.
   * @return core::ExecutionResult parse result.
   */
  static core::ExecutionResult ParseKeyData(
      const nlohmann::json& json_response, const std::string& key_data_tag,
      std::vector<std::shared_ptr<KeyData>>& key_data) noexcept;
};
}  // namespace google::scp::cpio::client_providers
