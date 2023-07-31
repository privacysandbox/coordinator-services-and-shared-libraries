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

#include "core/interface/errors.h"

namespace google::scp::core::errors {

REGISTER_COMPONENT_CODE(SC_CORE_UTILS, 0x0014)

DEFINE_ERROR_CODE(SC_CORE_UTILS_INVALID_INPUT, SC_CORE_UTILS, 0x0001,
                  "The input is invalid.", HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_CORE_UTILS_INVALID_BASE64_ENCODING_LENGTH, SC_CORE_UTILS,
                  0x0002, "The Base64 encoding is not a multiple of 4.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_CORE_UTILS_CURL_INIT_ERROR, SC_CORE_UTILS, 0x0003,
                  "CURL cannot be initialized.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

}  // namespace google::scp::core::errors
