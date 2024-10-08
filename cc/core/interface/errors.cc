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

#include "errors.h"

#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {
std::map<uint64_t, std::map<uint64_t, SCPError>>& GetGlobalErrorCodes() {
  /// Defines global_error_codes to store all error codes.
  static std::map<uint64_t, std::map<uint64_t, SCPError>> global_error_codes;
  return global_error_codes;
}

std::map<uint64_t, uint64_t>& GetPublicErrorCodesMap() {
  /// Defines public_error_codes_map to store error codes and associated public
  /// error code.
  static std::map<uint64_t, uint64_t> public_error_codes_map;
  return public_error_codes_map;
}

std::string HttpStatusCodeToString(HttpStatusCode status) {
  switch (status) {
    // 2xx
    case HttpStatusCode::OK:
      return "OK";
    case HttpStatusCode::CREATED:
      return "Created";
    case HttpStatusCode::ACCEPTED:
      return "Accepted";
    case HttpStatusCode::NO_CONTENT:
      return "No Content";
    case HttpStatusCode::PARTIAL_CONTENT:
      return "Partial Content";

    // 3xx
    case HttpStatusCode::MULTIPLE_CHOICES:
      return "Multiple Choices";
    case HttpStatusCode::MOVED_PERMANENTLY:
      return "Moved Permanently";
    case HttpStatusCode::FOUND:
      return "Found";
    case HttpStatusCode::SEE_OTHER:
      return "See Other";
    case HttpStatusCode::NOT_MODIFIED:
      return "Not Modified";
    case HttpStatusCode::TEMPORARY_REDIRECT:
      return "Temporary Redirect";
    case HttpStatusCode::PERMANENT_REDIRECT:
      return "Permanent Redirect";

    // 4xx
    case HttpStatusCode::BAD_REQUEST:
      return "Bad Request";
    case HttpStatusCode::UNAUTHORIZED:
      return "Unauthorized";
    case HttpStatusCode::FORBIDDEN:
      return "Forbidden";
    case HttpStatusCode::NOT_FOUND:
      return "Not Found";
    case HttpStatusCode::METHOD_NOT_ALLOWED:
      return "Method Not Allowed";
    case HttpStatusCode::REQUEST_TIMEOUT:
      return "Request Timeout";
    case HttpStatusCode::CONFLICT:
      return "Conflict";
    case HttpStatusCode::GONE:
      return "Gone";
    case HttpStatusCode::LENGTH_REQUIRED:
      return "Length Required";
    case HttpStatusCode::PRECONDITION_FAILED:
      return "Precondition Failed";
    case HttpStatusCode::REQUEST_ENTITY_TOO_LARGE:
      return "Request Entity Too Large";
    case HttpStatusCode::REQUEST_URI_TOO_LONG:
      return "Request URI Too Long";
    case HttpStatusCode::UNSUPPORTED_MEDIA_TYPE:
      return "Unsupported Media Type";
    case HttpStatusCode::REQUEST_RANGE_NOT_SATISFIABLE:
      return "Request Range Not Satisfiable";
    case HttpStatusCode::MISDIRECTED_REQUEST:
      return "Misdirected Request";
    case HttpStatusCode::TOO_MANY_REQUESTS:
      return "Too Many Requests";
    case HttpStatusCode::CANCELLED:
      return "Cancelled";

    // 5xx
    case HttpStatusCode::INTERNAL_SERVER_ERROR:
      return "Internal Server Error";
    case HttpStatusCode::NOT_IMPLEMENTED:
      return "Not Implemented";
    case HttpStatusCode::BAD_GATEWAY:
      return "Bad Gateway";
    case HttpStatusCode::SERVICE_UNAVAILABLE:
      return "Service Unavailable";
    case HttpStatusCode::GATEWAY_TIMEOUT:
      return "Gateway Timeout";
    case HttpStatusCode::HTTP_VERSION_NOT_SUPPORTED:
      return "HTTP Version Not Supported";

    // Default case for unknown status codes
    case HttpStatusCode::UNKNOWN:
      return "Unknown";
  }
}
}  // namespace google::scp::core::errors
