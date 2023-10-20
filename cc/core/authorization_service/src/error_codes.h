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

REGISTER_COMPONENT_CODE(SC_AUTHORIZATION_SERVICE, 0x0020)

DEFINE_ERROR_CODE(SC_AUTHORIZATION_SERVICE_BAD_TOKEN, SC_AUTHORIZATION_SERVICE,
                  0x0001, "Authorization token is malformed.",
                  HttpStatusCode::FORBIDDEN)

DEFINE_ERROR_CODE(SC_AUTHORIZATION_SERVICE_UNAUTHORIZED,
                  SC_AUTHORIZATION_SERVICE, 0x0002,
                  "Authorization forbidden or failed.",
                  HttpStatusCode::FORBIDDEN)

DEFINE_ERROR_CODE(SC_AUTHORIZATION_SERVICE_ACCESS_DENIED,
                  SC_AUTHORIZATION_SERVICE, 0x0003,
                  "Attempted access to resource is denied.",
                  HttpStatusCode::FORBIDDEN)

DEFINE_ERROR_CODE(SC_AUTHORIZATION_SERVICE_INVALID_CONFIG,
                  SC_AUTHORIZATION_SERVICE, 0x0004, "Invalid config.",
                  HttpStatusCode::FORBIDDEN)

DEFINE_ERROR_CODE(SC_AUTHORIZATION_SERVICE_INTERNAL_ERROR,
                  SC_AUTHORIZATION_SERVICE, 0x0005, "Unknown internal error.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_AUTHORIZATION_SERVICE_AUTH_TOKEN_IS_REFRESHING,
                  SC_AUTHORIZATION_SERVICE, 0x0006,
                  "The authentication token is being refreshed.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
}  // namespace google::scp::core::errors
