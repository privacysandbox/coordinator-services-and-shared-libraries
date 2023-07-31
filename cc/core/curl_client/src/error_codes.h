// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {

/// Registers component code as 0x0019 for CURL Client.
REGISTER_COMPONENT_CODE(SC_CURL_CLIENT, 0x0019);

DEFINE_ERROR_CODE(SC_CURL_CLIENT_CURL_INIT_ERROR, SC_CURL_CLIENT, 0x0001,
                  "Initializing CURL client failed",
                  HttpStatusCode::BAD_REQUEST);

DEFINE_ERROR_CODE(SC_CURL_CLIENT_CURL_HEADER_ADD_ERROR, SC_CURL_CLIENT, 0x0002,
                  "Adding a header to the CURL client failed",
                  HttpStatusCode::BAD_REQUEST);

DEFINE_ERROR_CODE(SC_CURL_CLIENT_POST_CONTENTS_TOO_LARGE, SC_CURL_CLIENT,
                  0x0003,
                  "Body of POST request is larger than the library supports",
                  HttpStatusCode::BAD_REQUEST);

DEFINE_ERROR_CODE(SC_CURL_CLIENT_BAD_HEADER_RECEIVED, SC_CURL_CLIENT, 0x0004,
                  "Header received from HTTP response is malformed",
                  HttpStatusCode::BAD_REQUEST);

DEFINE_ERROR_CODE(SC_CURL_CLIENT_UNSUPPORTED_METHOD, SC_CURL_CLIENT, 0x0005,
                  "HTTP Method is not supported by the CURL client",
                  HttpStatusCode::BAD_REQUEST);

DEFINE_ERROR_CODE(SC_CURL_CLIENT_REQUEST_FAILED, SC_CURL_CLIENT, 0x0006,
                  "HTTP request failed during execution",
                  HttpStatusCode::BAD_REQUEST);

DEFINE_ERROR_CODE(SC_CURL_CLIENT_NO_PATH_SUPPLIED, SC_CURL_CLIENT, 0x0007,
                  "No Path supplied to CURL client to request",
                  HttpStatusCode::BAD_REQUEST);

DEFINE_ERROR_CODE(SC_CURL_CLIENT_REQUEST_UNAUTHORIZED, SC_CURL_CLIENT, 0x0008,
                  "HTTP server returned UNAUTHORIZED",
                  HttpStatusCode::UNAUTHORIZED);

DEFINE_ERROR_CODE(SC_CURL_CLIENT_REQUEST_FORBIDDEN, SC_CURL_CLIENT, 0x0009,
                  "HTTP server returned FORBIDDEN", HttpStatusCode::FORBIDDEN);

DEFINE_ERROR_CODE(SC_CURL_CLIENT_REQUEST_NOT_FOUND, SC_CURL_CLIENT, 0x000A,
                  "HTTP server returned NOT_FOUND", HttpStatusCode::NOT_FOUND);

DEFINE_ERROR_CODE(SC_CURL_CLIENT_REQUEST_CONFLICT, SC_CURL_CLIENT, 0x000B,
                  "HTTP server returned CONFLICT", HttpStatusCode::CONFLICT);

DEFINE_ERROR_CODE(SC_CURL_CLIENT_REQUEST_SERVER_ERROR, SC_CURL_CLIENT, 0x000C,
                  "HTTP server returned INTERNAL_SERVER_ERROR",
                  HttpStatusCode::INTERNAL_SERVER_ERROR);

DEFINE_ERROR_CODE(SC_CURL_CLIENT_REQUEST_NOT_IMPLEMENTED, SC_CURL_CLIENT,
                  0x000D, "HTTP server returned NOT_IMPLEMENTED",
                  HttpStatusCode::NOT_IMPLEMENTED);

DEFINE_ERROR_CODE(SC_CURL_CLIENT_REQUEST_SERVICE_UNAVAILABLE, SC_CURL_CLIENT,
                  0x000E, "HTTP server returned SERVICE_UNAVAIALABLE",
                  HttpStatusCode::SERVICE_UNAVAILABLE);

DEFINE_ERROR_CODE(SC_CURL_CLIENT_REQUEST_OTHER_HTTP_ERROR, SC_CURL_CLIENT,
                  0x000F, "HTTP server returned unenumerated error",
                  HttpStatusCode::BAD_REQUEST);

DEFINE_ERROR_CODE(SC_CURL_CLIENT_REQUEST_BAD_REGEX_PARSING, SC_CURL_CLIENT,
                  0x0010, "Response code could not be parsed",
                  HttpStatusCode::BAD_REQUEST);

}  // namespace google::scp::core::errors
