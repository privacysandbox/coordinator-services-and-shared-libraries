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

#include "cc/core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {

REGISTER_COMPONENT_CODE(SC_HTTP2_SERVER, 0x000E)

DEFINE_ERROR_CODE(SC_HTTP2_SERVER_INVALID_HEADER, SC_HTTP2_SERVER, 0x0001,
                  "Http2Server invalid header value.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_HTTP2_SERVER_INVALID_METHOD, SC_HTTP2_SERVER, 0x0002,
                  "Http2Server invalid http method.",
                  HttpStatusCode::METHOD_NOT_ALLOWED)

DEFINE_ERROR_CODE(SC_HTTP2_SERVER_ALREADY_RUNNING, SC_HTTP2_SERVER, 0x0003,
                  "Http2Server already running.", HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_HTTP2_SERVER_ALREADY_STOPPED, SC_HTTP2_SERVER, 0x0004,
                  "Http2Server already stopped.", HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_HTTP2_SERVER_CANNOT_REGISTER_HANDLER, SC_HTTP2_SERVER,
                  0x0005, "Http2Server cannot register the handler.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_HTTP2_SERVER_INITIALIZATION_FAILED, SC_HTTP2_SERVER,
                  0x0006, "Http2Server initialization failed.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_HTTP2_SERVER_PARTIAL_REQUEST_BODY, SC_HTTP2_SERVER, 0x0007,
                  "Http2Server partial request body.",
                  HttpStatusCode::PARTIAL_CONTENT)

DEFINE_ERROR_CODE(SC_HTTP2_SERVER_BAD_REQUEST, SC_HTTP2_SERVER, 0x0008,
                  "Http2Server received bad request.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_HTTP2_SERVER_FAILED_TO_INITIALIZE_TLS_CONTEXT,
                  SC_HTTP2_SERVER, 0x0009,
                  "Http2Server failed to initialize TLS context.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_HTTP2_SERVER_FAILED_TO_RESOLVE_ROUTE, SC_HTTP2_SERVER,
                  0x000A, "Http2Server failed to resolve route for request.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_HTTP2_SERVER_FAILED_TO_ROUTE, SC_HTTP2_SERVER, 0x000B,
                  "Http2Server failed to route the request.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
}  // namespace google::scp::core::errors
