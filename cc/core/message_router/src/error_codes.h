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

/// Registers component code as 0x0003 for message router.
REGISTER_COMPONENT_CODE(SC_MESSAGE_ROUTER, 0x0003)

/// Defines the error code as 0x0001 when the request type is already
/// subscribed.
DEFINE_ERROR_CODE(SC_MESSAGE_ROUTER_REQUEST_ALREADY_SUBSCRIBED,
                  SC_MESSAGE_ROUTER, 0x0001,
                  "The request type has already been subscribed",
                  HttpStatusCode::BAD_REQUEST)

/// Defines the error code as 0x0002 when the request type is not subscribed.
DEFINE_ERROR_CODE(SC_MESSAGE_ROUTER_REQUEST_NOT_SUBSCRIBED, SC_MESSAGE_ROUTER,
                  0x0002, "The request type is not subscribed",
                  HttpStatusCode::BAD_REQUEST)
}  // namespace google::scp::core::errors
