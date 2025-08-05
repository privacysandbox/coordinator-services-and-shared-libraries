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
#include "cc/core/utils/src/http.h"

#include <string>
#include <utility>

#include <curl/curl.h>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "cc/core/utils/src/error_codes.h"
#include "cc/public/core/interface/execution_result.h"
#include "re2/re2.h"

namespace privacy_sandbox::pbs_common {

constexpr std::string_view kUserAgentPrefix = "aggregation-service/";

// Regular expression to match 'aggregation-service/x.y.z', where x, y, and z
// are digits.
constexpr LazyRE2 kVersionRegex = {R"(^([0-9]+\.[0-9]+\.[0-9]+))"};

ExecutionResultOr<std::string> GetEscapedUriWithQuery(
    const HttpRequest& request) {
  if (!request.query || request.query->empty()) {
    return *request.path;
  }

  CURL* curl_handle = curl_easy_init();
  if (!curl_handle) {
    return FailureExecutionResult(SC_CORE_UTILS_CURL_INIT_ERROR);
  }

  std::string escaped_query;
  // The "value" portion of each parameter needs to be escaped.
  for (const auto& query_part : absl::StrSplit(*request.query, "&")) {
    if (!escaped_query.empty()) absl::StrAppend(&escaped_query, "&");

    std::pair<std::string, std::string> name_and_value =
        (absl::StrSplit(query_part, "="));
    char* escaped_value =
        curl_easy_escape(curl_handle, name_and_value.second.c_str(),
                         name_and_value.second.length());
    absl::StrAppend(&escaped_query, name_and_value.first, "=", escaped_value);
    curl_free(escaped_value);
  }

  curl_easy_cleanup(curl_handle);
  return absl::StrCat(*request.path, "?", escaped_query);
}

ExecutionResultOr<std::string> ExtractRequestClaimedIdentity(
    const HttpHeaders& request_headers) noexcept {
  // Find the claimed identity header.
  auto header_iter = request_headers.find(kClaimedIdentityHeader);

  if (header_iter == request_headers.end()) {
    return FailureExecutionResult(SC_CORE_REQUEST_HEADER_NOT_FOUND);
  }
  return header_iter->second;
}

ExecutionResultOr<std::string> ExtractUserAgent(
    const HttpHeaders& request_headers) noexcept {
  // Find the User-Agent header.
  auto header_iter = request_headers.find(kUserAgentHeader);
  if (header_iter == request_headers.end()) {
    return FailureExecutionResult(SC_CORE_REQUEST_HEADER_NOT_FOUND);
  }

  const std::string& user_agent = header_iter->second;
  if (user_agent.empty() || !absl::StartsWith(user_agent, kUserAgentPrefix)) {
    return std::string(kUnknownValue);
  }

  // Search for the regex pattern in the User-Agent string.
  std::string_view match;
  if (!RE2::PartialMatch(std::string_view(&user_agent[kUserAgentPrefix.size()]),
                         *kVersionRegex, &match)) {
    return std::string(kUnknownValue);
  }
  return absl::StrCat(kUserAgentPrefix, match);
}

std::string HttpMethodToString(HttpMethod method) {
  switch (method) {
    case HttpMethod::GET:
      return "GET";
    case HttpMethod::POST:
      return "POST";
    case HttpMethod::PUT:
      return "PUT";
    case HttpMethod::UNKNOWN:
      return "UNKNOWN";
  }
  return "UNKNOWN";
}

}  // namespace privacy_sandbox::pbs_common
