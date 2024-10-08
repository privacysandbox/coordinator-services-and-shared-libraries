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

#include <string>

#include "core/interface/http_client_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::utils {
/**
 * @brief Get the Escaped URI from a HTTP request
 * Combines the path and query (after being escaped) in request and returns it.
 *
 * @param request
 * @return ExecutionResultOr<std::string>
 */
ExecutionResultOr<std::string> GetEscapedUriWithQuery(
    const HttpRequest& request);

/**
 * Extracts the claimed identity from the HTTP
 * request headers.
 *
 * @param request_headers A shared pointer to the HTTP headers. This contains
 * metadata about the request, including the claimed identity.
 *
 * @return ExecutionResultOr<absl::string_view> Returns an ExecutionResult
 * containing either the extracted claimed identity as an absl::string_view or
 * an error if the extraction fails. The function is marked noexcept to indicate
 * it doesn't throw exceptions.
 */
ExecutionResultOr<absl::string_view> ExtractRequestClaimedIdentity(
    const core::HttpHeaders& request_headers) noexcept;
}  // namespace google::scp::core::utils
