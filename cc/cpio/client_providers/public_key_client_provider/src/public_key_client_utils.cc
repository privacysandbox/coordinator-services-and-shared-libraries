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

#include "public_key_client_utils.h"

#include <locale>
#include <memory>
#include <regex>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/interface/http_types.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

#include "error_codes.h"

using google::cmrt::sdk::public_key_service::v1::PublicKey;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpHeaders;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::errors::
    SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED;
using google::scp::core::errors::
    SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED;
using std::get_time;
using std::istringstream;
using std::mktime;
using std::regex;
using std::regex_replace;
using std::stoi;
using std::tm;
using std::vector;

static constexpr char kPublicKeysLabel[] = "keys";
static constexpr char kPublicKeyIdLabel[] = "id";
static constexpr char kPublicKeyLabel[] = "key";
static constexpr char kPublicKeyHeaderDate[] = "date";
static constexpr char kPublicKeyHeaderCacheControl[] = "cache-control";
static constexpr char kPublicKeyDateTimeFormat[] = "%a, %d %b %Y %H:%M:%S";
static constexpr char kPublicKeyMaxAgePrefix[] = "max-age=";

namespace google::scp::cpio::client_providers {
ExecutionResult PublicKeyClientUtils::ParseExpiredTimeFromHeaders(
    const HttpHeaders& headers, uint64_t& expired_time_in_s) noexcept {
  auto created_date = headers.find(kPublicKeyHeaderDate);
  auto cache_control = headers.find(kPublicKeyHeaderCacheControl);
  if (created_date == headers.end() || cache_control == headers.end()) {
    return FailureExecutionResult(
        SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED);
  }
  tm time_date = {};
  istringstream stream_time(created_date->second);
  stream_time >> get_time(&time_date, kPublicKeyDateTimeFormat);
  auto max_age = regex_replace(cache_control->second,
                               std::regex(kPublicKeyMaxAgePrefix), "");

  auto mt_time = mktime(&time_date);
  if (mt_time < 0) {
    return FailureExecutionResult(
        SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED);
  }

  try {
    expired_time_in_s = mt_time + stoi(max_age);
  } catch (...) {
    return FailureExecutionResult(
        SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED);
  }

  return SuccessExecutionResult();
}

ExecutionResult PublicKeyClientUtils::ParsePublicKeysFromBody(
    const BytesBuffer& body, vector<PublicKey>& public_keys) noexcept {
  auto json_response =
      nlohmann::json::parse(body.bytes->begin(), body.bytes->end());
  auto json_keys = json_response.find(kPublicKeysLabel);
  if (json_keys == json_response.end()) {
    return FailureExecutionResult(
        SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED);
  }

  auto key_count = json_keys.value().size();
  for (size_t i = 0; i < key_count; ++i) {
    auto json_str = json_keys.value()[i];

    public_keys.emplace_back();
    auto it = json_str.find(kPublicKeyIdLabel);
    if (it == json_str.end()) {
      return FailureExecutionResult(
          SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED);
    }
    public_keys.back().set_key_id(it.value());

    it = json_str.find(kPublicKeyLabel);
    if (it == json_str.end()) {
      return FailureExecutionResult(
          SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED);
    }
    public_keys.back().set_public_key(it.value());
  }

  return SuccessExecutionResult();
}
}  // namespace google::scp::cpio::client_providers
