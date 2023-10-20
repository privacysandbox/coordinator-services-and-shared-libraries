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

/// Registers component code as 0x001A for concurrent queue.
REGISTER_COMPONENT_CODE(SC_STREAMING_CONTEXT, 0x001A)

DEFINE_ERROR_CODE(SC_STREAMING_CONTEXT_DONE, SC_STREAMING_CONTEXT, 0x0001,
                  "Streaming context is marked as done",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(SC_STREAMING_CONTEXT_CANCELLED, SC_STREAMING_CONTEXT, 0x0002,
                  "Streaming context is marked as cancelled",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

}  // namespace google::scp::core::errors
