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
#include "http.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <curl/curl.h>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "cc/public/core/interface/execution_result.h"

#include "error_codes.h"

using std::string;

namespace google::scp::core::utils {
ExecutionResultOr<std::string> GetEscapedUriWithQuery(
    const HttpRequest& request) {
  if (!request.query || request.query->empty()) {
    return *request.path;
  }

  CURL* curl_handle = curl_easy_init();
  if (!curl_handle) {
    return FailureExecutionResult(core::errors::SC_CORE_UTILS_CURL_INIT_ERROR);
  }

  string escaped_query;
  // The "value" portion of each parameter needs to be escaped.
  for (const auto& query_part : absl::StrSplit(*request.query, "&")) {
    if (!escaped_query.empty()) absl::StrAppend(&escaped_query, "&");

    std::pair<string, string> name_and_value =
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
}  // namespace google::scp::core::utils
