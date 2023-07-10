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
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {
REGISTER_COMPONENT_CODE(ARGUMENT_PARSER, 0x0301)

DEFINE_ERROR_CODE(ARGUMENT_PARSER_UNKNOWN_TYPE, ARGUMENT_PARSER, 0x0001,
                  "The JSON object mapping is unkown.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(ARGUMENT_PARSER_INVALID_JSON, ARGUMENT_PARSER, 0x0002,
                  "The JSON string is invalid.", HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(ARGUMENT_PARSER_INVALID_EXEC_ARG_JSON, ARGUMENT_PARSER,
                  0x0003, "Invalid JSON executable argument object.",
                  HttpStatusCode::BAD_REQUEST)
}  // namespace google::scp::core::errors
