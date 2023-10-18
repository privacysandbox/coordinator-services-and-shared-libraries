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

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::core::errors {
/// Error codes for Roma SharedMemory.
REGISTER_COMPONENT_CODE(SC_ROMA_SHARED_MEMORY, 0x0400)

DEFINE_ERROR_CODE(
    SC_ROMA_SHARED_MEMORY_MMAP_FAILURE, SC_ROMA_SHARED_MEMORY, 0x0001,
    "mmap() call failed, potentially bad argument or out of memory.",
    HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_ROMA_SHARED_MEMORY_INVALID_INIT, SC_ROMA_SHARED_MEMORY,
                  0x0002,
                  "Potentially initializing the shared memory more than once.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_ROMA_SHARED_MEMORY_UNMAP_FAILURE, SC_ROMA_SHARED_MEMORY,
                  0x0003, "munmap() call failed, potentially invalid argument.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

/// Error codes for Roma Process.
REGISTER_COMPONENT_CODE(SC_ROMA_PROCESS, 0x0401)

DEFINE_ERROR_CODE(SC_ROMA_PROCESS_CREATE_FAILURE, SC_ROMA_PROCESS, 0x0001,
                  "fork() call failed. Failed to create the child process.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

/// Error codes for Roma WorkQueue.
REGISTER_COMPONENT_CODE(SC_ROMA_WORK_QUEUE, 0x0402)

DEFINE_ERROR_CODE(SC_ROMA_WORK_QUEUE_POP_FAILURE, SC_ROMA_WORK_QUEUE, 0x0001,
                  "Failed to pop due to empty queue.",
                  HttpStatusCode::NO_CONTENT)

/// Error codes for Roma semaphore
REGISTER_COMPONENT_CODE(SC_ROMA_SEMAPHORE, 0x0403)
DEFINE_ERROR_CODE(SC_ROMA_SEMAPHORE_BAD_SEMAPHORE, SC_ROMA_SEMAPHORE, 0x0001,
                  "The semaphore is incorrectly initialized",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_ROMA_SEMAPHORE_WOULD_BLOCK, SC_ROMA_SEMAPHORE, 0x0002,
                  "The semaphore would block (try_wait failed)",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_ROMA_SEMAPHORE_TIMED_OUT, SC_ROMA_SEMAPHORE, 0x0003,
                  "The semaphore wait failed with time out.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
}  // namespace google::scp::core::errors
