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

#include "cc/core/interface/async_context.h"
#include "cc/core/interface/http_types.h"
#include "cc/public/core/interface/execution_result.h"

namespace privacy_sandbox::pbs_common {
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
ExecutionResultOr<std::string> ExtractRequestClaimedIdentity(
    const HttpHeaders& request_headers) noexcept;

/**
 * Retrieves the claimed identity from the HTTP request headers, if available.
 * This function works with both NgHttp2Request and HttpRequest types since it
 * is a template function. If the extraction fails or headers are missing, it
 * returns a predefined "unknown" value.
 *
 * @tparam RequestType The type of the HTTP request. It can be NgHttp2Request
 * or HttpRequest, or any class derived from HttpRequest.
 * @tparam ResponseType The type of the HTTP response.
 *
 * @param http_context A constant reference to the AsyncContext object
 * containing the request and response. The request holds the headers from
 * which the claimed identity is extracted.
 *
 * @return std::string The claimed identity if extraction is successful. If
 * headers are missing or extraction fails, the function returns a default
 * "unknown" value (kUnknownValue).
 *
 * @note This function ensures safe access to headers by checking for null
 * pointers. It also handles both NgHttp2Request and HttpRequest uniformly.
 */
template <typename RequestType, typename ResponseType>
std::string GetClaimedIdentityOrUnknownValue(
    const AsyncContext<RequestType, ResponseType>& http_context) {
  if (http_context.request && http_context.request->headers) {
    ExecutionResultOr<std::string> claimed_identity_extraction_result =
        ExtractRequestClaimedIdentity(*http_context.request->headers);

    if (claimed_identity_extraction_result.Successful()) {
      return *claimed_identity_extraction_result;
    }
  }

  // Return the "unknown" value if headers are missing or extraction fails.
  return std::string(kUnknownValue);
}

/**
 * Extracts the User Agent from the HTTP request headers.
 *
 * @param request_headers A shared pointer to the HTTP headers. This contains
 * metadata about the request, including the claimed identity.
 *
 * @return ExecutionResultOr<absl::string_view> Returns an ExecutionResult
 * containing either the extracted claimed identity as an absl::string_view or
 * an error if the extraction fails. The function is marked noexcept to indicate
 * it doesn't throw exceptions.
 */
ExecutionResultOr<std::string> ExtractUserAgent(
    const HttpHeaders& request_headers) noexcept;

/**
 * Extracts the client version from the User-Agent header in the
 * HTTP request, if available. This function works with both NgHttp2Request and
 * HttpRequest types, allowing flexibility through template specialization. If
 * the User-Agent extraction fails or headers are missing, it returns a
 * predefined "unknown" value.
 *
 * @tparam RequestType The type of the HTTP request, which can be NgHttp2Request
 * or HttpRequest, or any derived class from HttpRequest.
 * @tparam ResponseType The type of the HTTP response.
 *
 * @param http_context A constant reference to the AsyncContext containing the
 * request and response. The request holds the headers from which the User-Agent
 * (client version) is extracted.
 *
 * @return std::string The extracted client version (User-Agent) if successful.
 * If headers are missing or extraction fails, the function returns a default
 * "unknown" value (kUnknownValue).
 *
 * @note This function ensures safe access to headers by checking for null
 * pointers, and it is designed to work with both NgHttp2Request and HttpRequest
 * via template specialization.
 */
template <typename RequestType, typename ResponseType>
std::string GetUserAgentOrUnknownValue(
    const AsyncContext<RequestType, ResponseType>& http_context) {
  if (http_context.request && http_context.request->headers) {
    ExecutionResultOr<std::string> trusted_client_version_extraction_result =
        ExtractUserAgent(*http_context.request->headers);

    if (trusted_client_version_extraction_result.Successful()) {
      return *trusted_client_version_extraction_result;
    }
  }

  // Return the default "unknown" value if extraction fails or headers are
  // missing.
  return std::string(kUnknownValue);
}

// Function to convert HttpMethod enum to a string representation
std::string HttpMethodToString(HttpMethod method);
}  // namespace privacy_sandbox::pbs_common
