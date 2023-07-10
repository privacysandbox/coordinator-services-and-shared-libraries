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

/// Registers component code as 0x0002 for config provider.
REGISTER_COMPONENT_CODE(SC_CONFIG_PROVIDER, 0x0002)

/// Defines config file parsing error code as 0x0001.
DEFINE_ERROR_CODE(SC_CONFIG_PROVIDER_CANNOT_PARSE_CONFIG_FILE,
                  SC_CONFIG_PROVIDER, 0x0001,
                  "Config provider cannot load config file",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

/// Defines the error code as 0x0002 when the key cannot find.
DEFINE_ERROR_CODE(SC_CONFIG_PROVIDER_KEY_NOT_FOUND, SC_CONFIG_PROVIDER, 0x0002,
                  "Config provider cannot find the key",
                  HttpStatusCode::NOT_FOUND)

/// Defines the error code as 0x0003 when value type doesn't match.
DEFINE_ERROR_CODE(SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR, SC_CONFIG_PROVIDER,
                  0x0003, "Config provider value type of the key doesn't match",
                  HttpStatusCode::BAD_REQUEST)
}  // namespace google::scp::core::errors
