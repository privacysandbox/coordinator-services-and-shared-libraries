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

/// Registers component code as 0x0024 for SysLogProvider.
REGISTER_COMPONENT_CODE(SC_SYSLOG_PROVIDER, 0x0024)

/// Defines the error code as 0x0001 when an error has occured while opening a
/// connection to syslog.
DEFINE_ERROR_CODE(SC_SYSLOG_OPEN_CONNECTION_ERROR, SC_SYSLOG_PROVIDER, 0x0001,
                  "Error opening connection to syslog",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

/// Defines the error code as 0x0002 when an error has occured while closing a
/// connection to syslog.
DEFINE_ERROR_CODE(SC_SYSLOG_CLOSE_CONNECTION_ERROR, SC_SYSLOG_PROVIDER, 0x0002,
                  "Error closing connection to syslog",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

}  // namespace google::scp::core::errors
