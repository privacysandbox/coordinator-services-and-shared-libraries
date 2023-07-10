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

/// Registers component code as 0x0001 for concurrent queue.
REGISTER_COMPONENT_CODE(SC_CONCURRENT_QUEUE, 0x0001)

/// Defines enqueue error code as 0x0001.
DEFINE_ERROR_CODE(SC_CONCURRENT_QUEUE_CANNOT_ENQUEUE, SC_CONCURRENT_QUEUE,
                  0x0001, "Concurrent queue cannot enqueue",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

/// Defines dequeue error code as 0x0002.
DEFINE_ERROR_CODE(SC_CONCURRENT_QUEUE_CANNOT_DEQUEUE, SC_CONCURRENT_QUEUE,
                  0x0002, "Concurrent queue cannot dequeue",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

}  // namespace google::scp::core::errors
