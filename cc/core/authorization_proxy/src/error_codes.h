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

REGISTER_COMPONENT_CODE(SC_AUTHORIZATION_PROXY, 0x0021)

DEFINE_ERROR_CODE(SC_AUTHORIZATION_PROXY_BAD_REQUEST, SC_AUTHORIZATION_PROXY,
                  0x0001, "Authorization request is malformed.",
                  HttpStatusCode::FORBIDDEN)

DEFINE_ERROR_CODE(SC_AUTHORIZATION_PROXY_UNAUTHORIZED, SC_AUTHORIZATION_PROXY,
                  0x0002, "Authorization forbidden or failed.",
                  HttpStatusCode::FORBIDDEN)

DEFINE_ERROR_CODE(SC_AUTHORIZATION_PROXY_INVALID_CONFIG, SC_AUTHORIZATION_PROXY,
                  0x0003, "Invalid configuration.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_AUTHORIZATION_PROXY_REMOTE_UNAVAILABLE,
                  SC_AUTHORIZATION_PROXY, 0x0004, "Invalid configuration.",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(
    SC_AUTHORIZATION_PROXY_AUTH_REQUEST_INPROGRESS, SC_AUTHORIZATION_PROXY,
    0x0005,
    "An existing authentication request for the same is being processed.",
    HttpStatusCode::INTERNAL_SERVER_ERROR)
}  // namespace google::scp::core::errors
