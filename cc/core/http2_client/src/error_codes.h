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
/// Registers component code as 0x0012 for HTTP2 Client.
REGISTER_COMPONENT_CODE(SC_HTTP2_CLIENT, 0x0012)

DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_INVALID_URI, SC_HTTP2_CLIENT, 0x0001,
                  "The URI is invalid", HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_FAILED_TO_CONNECT, SC_HTTP2_CLIENT, 0x0002,
                  "Failed to connect to the HTTP server",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_FAILED_TO_ISSUE_HTTP_REQUEST, SC_HTTP2_CLIENT,
                  0x0003, "Failed to issue HTTP request.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_NOT_OK_RESPONSE, SC_HTTP2_CLIENT, 0x0004,
                  "The http response code is not OK",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_AUTH_NO_HEADER_SPECIFIED, SC_HTTP2_CLIENT,
                  0x0005, "No headers to sign specified.",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_AUTH_MISSING_HEADER, SC_HTTP2_CLIENT, 0x0006,
                  "Missing required header from http request.",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_AUTH_BAD_REQUEST, SC_HTTP2_CLIENT, 0x0007,
                  "Bad http request for signing.", HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_AUTH_ALREADY_SIGNED, SC_HTTP2_CLIENT, 0x0008,
                  "Http request seems to be already signed.",
                  HttpStatusCode::BAD_REQUEST)
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_TLS_CTX_ERROR, SC_HTTP2_CLIENT, 0x0009,
                  "TLS context error", HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_NO_CONNECTION_ESTABLISHED, SC_HTTP2_CLIENT,
                  0x000A, "No connection has been established yet.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_METHOD_NOT_SUPPORTED, SC_HTTP2_CLIENT,
                  0x000B, "The http method is not supported.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_NOT_OK_RESPONSE_BUT_RETRIABLE,
                  SC_HTTP2_CLIENT, 0x000C,
                  "The http response code is not OK but retriable",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_CONNECTION_POOL_IS_NOT_AVAILABLE,
                  SC_HTTP2_CLIENT, 0x000D,
                  "The connection pool is not available",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_CONNECTION_INITIALIZATION_FAILED,
                  SC_HTTP2_CLIENT, 0x000E,
                  "The connection cannot be initialized",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_CONNECTION_STOP_FAILED, SC_HTTP2_CLIENT,
                  0x000F, "The connection cannot be stopped",
                  HttpStatusCode::INTERNAL_SERVER_ERROR);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_CONNECTION_DROPPED, SC_HTTP2_CLIENT, 0x0010,
                  "The connection was dropped.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_MULTIPLE_CHOICES, SC_HTTP2_CLIENT,
                  0x0015, "HttpStatus Code: MULTIPLE_CHOICES",
                  HttpStatusCode::MULTIPLE_CHOICES);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_MOVED_PERMANENTLY,
                  SC_HTTP2_CLIENT, 0x0016, "HttpStatus Code: MOVED_PERMANENTLY",
                  HttpStatusCode::MOVED_PERMANENTLY);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_FOUND, SC_HTTP2_CLIENT, 0x0017,
                  "HttpStatus Code: FOUND", HttpStatusCode::FOUND);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_SEE_OTHER, SC_HTTP2_CLIENT,
                  0x0018, "HttpStatus Code: SEE_OTHER",
                  HttpStatusCode::SEE_OTHER);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_NOT_MODIFIED, SC_HTTP2_CLIENT,
                  0x0019, "HttpStatus Code: NOT_MODIFIED",
                  HttpStatusCode::NOT_MODIFIED);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_TEMPORARY_REDIRECT,
                  SC_HTTP2_CLIENT, 0x001A,
                  "HttpStatus Code: TEMPORARY_REDIRECT",
                  HttpStatusCode::TEMPORARY_REDIRECT);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_PERMANENT_REDIRECT,
                  SC_HTTP2_CLIENT, 0x001B,
                  "HttpStatus Code: PERMANENT_REDIRECT",
                  HttpStatusCode::PERMANENT_REDIRECT);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_BAD_REQUEST, SC_HTTP2_CLIENT,
                  0x001C, "HttpStatus Code: BAD_REQUEST",
                  HttpStatusCode::BAD_REQUEST);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_UNAUTHORIZED, SC_HTTP2_CLIENT,
                  0x001D, "HttpStatus Code: UNAUTHORIZED",
                  HttpStatusCode::UNAUTHORIZED);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_FORBIDDEN, SC_HTTP2_CLIENT,
                  0x001E, "HttpStatus Code: FORBIDDEN",
                  HttpStatusCode::FORBIDDEN);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_NOT_FOUND, SC_HTTP2_CLIENT,
                  0x0020, "HttpStatus Code: NOT_FOUND",
                  HttpStatusCode::NOT_FOUND);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_METHOD_NOT_ALLOWED,
                  SC_HTTP2_CLIENT, 0x0021,
                  "HttpStatus Code: METHOD_NOT_ALLOWED",
                  HttpStatusCode::METHOD_NOT_ALLOWED);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_REQUEST_TIMEOUT, SC_HTTP2_CLIENT,
                  0x0022, "HttpStatus Code: REQUEST_TIMEOUT",
                  HttpStatusCode::REQUEST_TIMEOUT);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_CONFLICT, SC_HTTP2_CLIENT, 0x0023,
                  "HttpStatus Code: CONFLICT", HttpStatusCode::CONFLICT);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_GONE, SC_HTTP2_CLIENT, 0x0024,
                  "HttpStatus Code: GONE", HttpStatusCode::GONE);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_LENGTH_REQUIRED, SC_HTTP2_CLIENT,
                  0x0025, "HttpStatus Code: LENGTH_REQUIRED",
                  HttpStatusCode::LENGTH_REQUIRED);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_PRECONDITION_FAILED,
                  SC_HTTP2_CLIENT, 0x0026,
                  "HttpStatus Code: PRECONDITION_FAILED",
                  HttpStatusCode::PRECONDITION_FAILED);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE,
                  SC_HTTP2_CLIENT, 0x0027,
                  "HttpStatus Code: REQUEST_ENTITY_TOO_LARGE",
                  HttpStatusCode::REQUEST_ENTITY_TOO_LARGE);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_REQUEST_URI_TOO_LONG,
                  SC_HTTP2_CLIENT, 0x0028,
                  "HttpStatus Code: REQUEST_URI_TOO_LONG",
                  HttpStatusCode::REQUEST_URI_TOO_LONG);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE,
                  SC_HTTP2_CLIENT, 0x0029,
                  "HttpStatus Code: UNSUPPORTED_MEDIA_TYPE",
                  HttpStatusCode::UNSUPPORTED_MEDIA_TYPE);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_REQUEST_RANGE_NOT_SATISFIABLE,
                  SC_HTTP2_CLIENT, 0x002A,
                  "HttpStatus Code: REQUEST_RANGE_NOT_SATISFIABLE",
                  HttpStatusCode::REQUEST_RANGE_NOT_SATISFIABLE);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_MISDIRECTED_REQUEST,
                  SC_HTTP2_CLIENT, 0x002B,
                  "HttpStatus Code: MISDIRECTED_REQUEST",
                  HttpStatusCode::MISDIRECTED_REQUEST);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_TOO_MANY_REQUESTS,
                  SC_HTTP2_CLIENT, 0x002C, "HttpStatus Code: TOO_MANY_REQUESTS",
                  HttpStatusCode::TOO_MANY_REQUESTS);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_INTERNAL_SERVER_ERROR,
                  SC_HTTP2_CLIENT, 0x002D,
                  "HttpStatus Code: INTERNAL_SERVER_ERROR",
                  HttpStatusCode::INTERNAL_SERVER_ERROR);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_NOT_IMPLEMENTED, SC_HTTP2_CLIENT,
                  0x002E, "HttpStatus Code: NOT_IMPLEMENTED",
                  HttpStatusCode::NOT_IMPLEMENTED);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_BAD_GATEWAY, SC_HTTP2_CLIENT,
                  0x002F, "HttpStatus Code: BAD_GATEWAY",
                  HttpStatusCode::BAD_GATEWAY);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_SERVICE_UNAVAILABLE,
                  SC_HTTP2_CLIENT, 0x0030,
                  "HttpStatus Code: SERVICE_UNAVAILABLE",
                  HttpStatusCode::SERVICE_UNAVAILABLE);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_GATEWAY_TIMEOUT, SC_HTTP2_CLIENT,
                  0x0031, "HttpStatus Code: GATEWAY_TIMEOUT",
                  HttpStatusCode::GATEWAY_TIMEOUT);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED,
                  SC_HTTP2_CLIENT, 0x0032,
                  "HttpStatus Code: HTTP_VERSION_NOT_SUPPORTED",
                  HttpStatusCode::HTTP_VERSION_NOT_SUPPORTED);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_REQUEST_CLOSE_ERROR, SC_HTTP2_CLIENT,
                  0x0033, "nghttp2 request on_close got error",
                  HttpStatusCode::INTERNAL_SERVER_ERROR);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_REQUEST_RESPONSE_STATUS_UNKNOWN,
                  SC_HTTP2_CLIENT, 0x0034,
                  "nghttp2 request response status unknown",
                  HttpStatusCode::INTERNAL_SERVER_ERROR);
DEFINE_ERROR_CODE(SC_HTTP2_CLIENT_HTTP_CONNECTION_NOT_READY, SC_HTTP2_CLIENT,
                  0x0035, "Http connection is not ready",
                  HttpStatusCode::INTERNAL_SERVER_ERROR);
}  // namespace google::scp::core::errors
