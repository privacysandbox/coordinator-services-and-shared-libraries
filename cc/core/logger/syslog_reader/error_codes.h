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

/// Registers component code as 0x0111 for syslog reader.
REGISTER_COMPONENT_CODE(SC_SYSLOG_READER, 0x0111)

/// Defines the error code as 0x0111 when an error occurs opening the log file
DEFINE_ERROR_CODE(SC_SYSLOG_READER_ERROR_OPENING_LOG_FILE, SC_SYSLOG_READER,
                  0x0001, "There was an error opening the log file",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

/// Defines the error code as 0x0111 when an error occurs when seeking to the
/// offset in the log file
DEFINE_ERROR_CODE(SC_SYSLOG_READER_ERROR_SEEKING_LOG_FILE, SC_SYSLOG_READER,
                  0x0002,
                  "There was an error seeking to the offset in the log file",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

}  // namespace google::scp::core::errors
