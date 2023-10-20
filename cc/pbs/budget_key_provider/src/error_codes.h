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

#include "cc/core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {

REGISTER_COMPONENT_CODE(SC_BUDGET_KEY_PROVIDER, 0x0102)

DEFINE_ERROR_CODE(SC_BUDGET_KEY_PROVIDER_ENTRY_IS_LOADING,
                  SC_BUDGET_KEY_PROVIDER, 0x0001, "The entry is being loaded.",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(SC_BUDGET_KEY_PROVIDER_INVALID_OPERATION_TYPE,
                  SC_BUDGET_KEY_PROVIDER, 0x0002, "Invalid operation type.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_BUDGET_KEY_PROVIDER_CANNOT_STOP_COMPLETELY,
                  SC_BUDGET_KEY_PROVIDER, 0x0003,
                  "Cannot be stopped completely.",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

}  // namespace google::scp::core::errors
