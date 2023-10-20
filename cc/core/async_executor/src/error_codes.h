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

#pragma once

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {

/// Registers component code as 0x0006 for async executor.
REGISTER_COMPONENT_CODE(SC_ASYNC_EXECUTOR, 0x0006)

DEFINE_ERROR_CODE(SC_ASYNC_EXECUTOR_INVALID_QUEUE_CAP, SC_ASYNC_EXECUTOR,
                  0x0001, "The queue cap is invalid",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ASYNC_EXECUTOR_INVALID_THREAD_COUNT, SC_ASYNC_EXECUTOR,
                  0x0002, "The thread count is invalid",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ASYNC_EXECUTOR_EXCEEDING_QUEUE_CAP, SC_ASYNC_EXECUTOR,
                  0x0003, "The work queue length is exceeding the cap",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ASYNC_EXECUTOR_NOT_INITIALIZED, SC_ASYNC_EXECUTOR, 0x0004,
                  "The executor is not initialized yet",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_ASYNC_EXECUTOR_ALREADY_RUNNING, SC_ASYNC_EXECUTOR, 0x0005,
                  "The executor is already running",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ASYNC_EXECUTOR_NOT_RUNNING, SC_ASYNC_EXECUTOR, 0x0006,
                  "The executor is not running yet",
                  HttpStatusCode::SERVICE_UNAVAILABLE)

DEFINE_ERROR_CODE(SC_ASYNC_EXECUTOR_INVALID_PRIORITY_TYPE, SC_ASYNC_EXECUTOR,
                  0x0007, "Invalid priority type.", HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ASYNC_EXECUTOR_INVALID_LOAD_BALANCING_TYPE,
                  SC_ASYNC_EXECUTOR, 0x0008, "Invalid load balancing type.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ASYNC_EXECUTOR_INVALID_TASK_POOL_TYPE, SC_ASYNC_EXECUTOR,
                  0x0009, "Invalid task pool type.",
                  HttpStatusCode::BAD_REQUEST)

DEFINE_ERROR_CODE(SC_ASYNC_EXECUTOR_UNABLE_TO_SET_AFFINITY, SC_ASYNC_EXECUTOR,
                  0x000A, "Setting CPU affinity failed",
                  HttpStatusCode::BAD_REQUEST)
}  // namespace google::scp::core::errors
