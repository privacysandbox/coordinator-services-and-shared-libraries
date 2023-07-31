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

/// Registers component code as 0x250 for auto refresh token provider
REGISTER_COMPONENT_CODE(SC_AUTO_REFRESH_TOKEN_PROVIDER, 0x250)

DEFINE_ERROR_CODE(SC_AUTO_REFRESH_TOKEN_PROVIDER_FAILED_TO_STOP,
                  SC_AUTO_REFRESH_TOKEN_PROVIDER, 0x0001,
                  "Failed to stop auto refresh token provider.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_AUTO_REFRESH_TOKEN_PROVIDER_TOKEN_NOT_AVAILABLE,
                  SC_AUTO_REFRESH_TOKEN_PROVIDER, 0x0002,
                  "Failed to fetch token.", HttpStatusCode::SERVICE_UNAVAILABLE)

}  // namespace google::scp::core::errors
