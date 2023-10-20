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
/// Registers component code as 0x0008 for NoSQL database provider.
REGISTER_COMPONENT_CODE(SC_NO_SQL_DATABASE_PROVIDER, 0x0008)

DEFINE_ERROR_CODE(SC_NO_SQL_DATABASE_PROVIDER_TABLE_NOT_FOUND,
                  SC_NO_SQL_DATABASE_PROVIDER, 0x0001,
                  "NoSQL Database table not found.", HttpStatusCode::NOT_FOUND)

DEFINE_ERROR_CODE(SC_NO_SQL_DATABASE_PROVIDER_INVALID_PARTITION_KEY_NAME,
                  SC_NO_SQL_DATABASE_PROVIDER, 0x0002,
                  "NoSQL Database invalid partition key name.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_NO_SQL_DATABASE_PROVIDER_INVALID_SORT_KEY_NAME,
                  SC_NO_SQL_DATABASE_PROVIDER, 0x0003,
                  "NoSQL Database invalid sort key name.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_NO_SQL_DATABASE_PROVIDER_RECORD_NOT_FOUND,
                  SC_NO_SQL_DATABASE_PROVIDER, 0x0004,
                  "NoSQL Database record not found.", HttpStatusCode::NOT_FOUND)

DEFINE_ERROR_CODE(SC_NO_SQL_DATABASE_INVALID_REQUEST,
                  SC_NO_SQL_DATABASE_PROVIDER, 0x0005,
                  "Invalid NoSQL Database request.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_NO_SQL_DATABASE_INVALID_PARAMETER_TYPE,
                  SC_NO_SQL_DATABASE_PROVIDER, 0x0006,
                  "Invalid NoSQL Database parameter type.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(
    SC_NO_SQL_DATABASE_UNRETRIABLE_ERROR, SC_NO_SQL_DATABASE_PROVIDER, 0x0007,
    "Invalid NoSQL Database request failed and cannot be retried.",
    HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_NO_SQL_DATABASE_RETRIABLE_ERROR,
                  SC_NO_SQL_DATABASE_PROVIDER, 0x0008,
                  "Invalid NoSQL Database request failed and can be retried.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_NO_SQL_DATABASE_JSON_FAILED_TO_PARSE,
                  SC_NO_SQL_DATABASE_PROVIDER, 0x0009,
                  "Failed to parse JSON value from Spanner query.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_NO_SQL_DATABASE_PROVIDER_RECORD_CORRUPTED,
                  SC_NO_SQL_DATABASE_PROVIDER, 0x000A,
                  "NoSQL Database record corrupted.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

}  // namespace google::scp::core::errors
