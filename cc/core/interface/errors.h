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

#include <functional>
#include <map>
#include <string>

#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {

/// Enumerator for all the http status codes.
enum class HttpStatusCode {
  UNKNOWN = 0,

  // 2xx
  OK = 200,
  CREATED = 201,
  ACCEPTED = 202,
  NO_CONTENT = 204,
  PARTIAL_CONTENT = 206,

  // 3xx
  MULTIPLE_CHOICES = 300,
  MOVED_PERMANENTLY = 301,
  FOUND = 302,
  SEE_OTHER = 303,
  NOT_MODIFIED = 304,
  TEMPORARY_REDIRECT = 307,
  PERMANENT_REDIRECT = 308,

  // 4xx
  BAD_REQUEST = 400,
  UNAUTHORIZED = 401,
  FORBIDDEN = 403,
  NOT_FOUND = 404,
  METHOD_NOT_ALLOWED = 405,
  REQUEST_TIMEOUT = 408,
  CONFLICT = 409,
  GONE = 410,
  LENGTH_REQUIRED = 411,
  PRECONDITION_FAILED = 412,
  REQUEST_ENTITY_TOO_LARGE = 413,
  REQUEST_URI_TOO_LONG = 414,
  UNSUPPORTED_MEDIA_TYPE = 415,
  REQUEST_RANGE_NOT_SATISFIABLE = 416,
  MISDIRECTED_REQUEST = 421,
  TOO_MANY_REQUESTS = 429,
  CANCELLED = 444,

  // 5xx
  INTERNAL_SERVER_ERROR = 500,
  NOT_IMPLEMENTED = 501,
  BAD_GATEWAY = 502,
  SERVICE_UNAVAILABLE = 503,
  GATEWAY_TIMEOUT = 504,
  HTTP_VERSION_NOT_SUPPORTED = 505
};

inline bool IsRetriableErrorCode(HttpStatusCode http_status_code) {
  return http_status_code >= HttpStatusCode::INTERNAL_SERVER_ERROR;
}

/**
 * @brief Extern links global_error_codes that stores all error codes.
 *
 * It is a map from the component_code to the map between error codes and error
 * messages.
 *
 */
struct SCPError {
  std::string error_message;
  HttpStatusCode error_http_status_code;
};

std::map<uint64_t, std::map<uint64_t, SCPError>>& GetGlobalErrorCodes();

/// @brief The global map of error_code and associated public error code.
std::map<uint64_t, uint64_t>& GetPublicErrorCodesMap();

/**
 * @brief Registers component code.
 *
 * @param component_name component name.
 * @param component_code a uint_64 number.
 */
#define REGISTER_COMPONENT_CODE(component_name, component_code) \
  static constexpr uint64_t component_name = component_code;    \
  static constexpr bool registered_##component_code = true;

/**
 * @brief Makes a global unique error code with the component code and the
 * component-specific error code.
 *
 * @param component component code.
 * @param error component-specific error code.
 */
inline uint64_t MakeErrorCode(uint64_t component, uint64_t error) {
  return (((uint64_t)(1) << 31) | ((uint64_t)(component) << 16) |
          ((uint64_t)(error)));
}

/**
 * @brief Defines an error code and emplaces the error code in
 * global_error_codes.
 *
 * @param error_name the global error code name.
 * @param component component code.
 * @param error component-specific error code.
 * @param message message about the error.
 * @param http_status_code Http status code.
 */
#define DEFINE_ERROR_CODE(error_name, component, error, message,              \
                          http_status_code)                                   \
  static_assert(                                                              \
      component < 0x8000,                                                     \
      "Component code is too large! Valid range is [0x0001, 0x7FFF).");       \
  static_assert(error < 0x10000,                                              \
                "Error code is too large! Valid range is [0x0001, 0xFFFF]."); \
  static uint64_t error_name =                                                \
      ::google::scp::core::errors::MakeErrorCode(component, error);           \
  static bool initialized_##component##error = []() {                         \
    ::google::scp::core::errors::SCPError scp_error;                          \
    scp_error.error_message = message;                                        \
    scp_error.error_http_status_code = http_status_code;                      \
    ::google::scp::core::errors::GetGlobalErrorCodes()[component].emplace(    \
        error_name, scp_error);                                               \
    return true;                                                              \
  }();

/**
 * @brief Maps internal error code to public error code.
 *
 * @param error_code The global error code.
 * @param public_error_code public error code associated with this error.
 */
#define MAP_TO_PUBLIC_ERROR_CODE(error_code, public_error_code)    \
  static bool mapped_##error_code = []() {                         \
    ::google::scp::core::errors::GetPublicErrorCodesMap().emplace( \
        error_code, public_error_code);                            \
    return true;                                                   \
  }();

/**
 * @brief Extracts component code object.
 *
 * @param error_code error code.
 * @return uint64_t component code.
 */
inline uint64_t ExtractComponentCode(uint64_t error_code) {
  return ((error_code >> 16) & 0x7FFF);
}

/**
 * @brief Gets the error message.
 *
 * @param error_code the global error code.
 * @return std::string the message about the error code.
 */
inline const char* GetErrorMessage(uint64_t error_code) {
  static constexpr char kInvalidErrorCodeStr[] = "InvalidErrorCode";
  static constexpr char kUnknownErrorCodeStr[] = "Unknown Error";
  static constexpr char kSuccessErrorCodeStr[] = "Success";

  if (error_code == SC_OK) {
    return kSuccessErrorCodeStr;
  } else if (error_code == SC_UNKNOWN) {
    return kUnknownErrorCodeStr;
  }

  uint64_t component = ExtractComponentCode(error_code);
  auto it = GetGlobalErrorCodes()[component].find(error_code);
  if (it != GetGlobalErrorCodes()[component].end()) {
    return it->second.error_message.c_str();
  }

  return kInvalidErrorCodeStr;
}

/**
 * @brief Gets the error http status code.
 *
 * @param error_code The global error code.
 * @return HttpStatusCode The http status code associated with the error.
 */
inline HttpStatusCode GetErrorHttpStatusCode(uint64_t error_code) {
  uint64_t component = ExtractComponentCode(error_code);
  return GetGlobalErrorCodes()[component]
      .find(error_code)
      ->second.error_http_status_code;
}

/**
 * @brief Gets the public error code.
 *
 * @param error_code The global error code.
 * @return uint64_t The public error code associated with the error.
 */
inline uint64_t GetPublicErrorCode(uint64_t error_code) {
  if (error_code == SC_OK) {
    return SC_OK;
  }
  auto it = GetPublicErrorCodesMap().find(error_code);
  if (it == GetPublicErrorCodesMap().end()) {
    // Invalid error code.
    return SC_UNKNOWN;
  }
  return it->second;
}

}  // namespace google::scp::core::errors
