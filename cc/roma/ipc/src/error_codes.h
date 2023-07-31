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
REGISTER_COMPONENT_CODE(SC_ROMA_IPC_MANAGER, 0x0480)
DEFINE_ERROR_CODE(SC_ROMA_IPC_MANAGER_INVALID_INDEX, SC_ROMA_IPC_MANAGER,
                  0x0001, "The worker index is out of range.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

DEFINE_ERROR_CODE(SC_ROMA_IPC_MANAGER_BAD_DISPATCHER_ROLE, SC_ROMA_IPC_MANAGER,
                  0x0002, "The caller role should be dispatcher.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
DEFINE_ERROR_CODE(SC_ROMA_IPC_MANAGER_BAD_WORKER_ROLE, SC_ROMA_IPC_MANAGER,
                  0x0003, "The caller role should be worker.",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)

REGISTER_COMPONENT_CODE(SC_ROMA_IPC_MESSAGE, 0x0482)
DEFINE_ERROR_CODE(SC_ROMA_IPC_MESSAGE_REQUEST_TAG_NOT_FOUND,
                  SC_ROMA_IPC_MESSAGE, 0x0001,
                  "Cannot find the tag in the request.",
                  HttpStatusCode::NOT_FOUND)

REGISTER_COMPONENT_CODE(SC_ROMA_WORK_CONTAINER, 0x0483)
DEFINE_ERROR_CODE(SC_ROMA_WORK_CONTAINER_STOPPED, SC_ROMA_WORK_CONTAINER,
                  0x0001, "Work container stopped and released locks.",
                  HttpStatusCode::NOT_FOUND)

REGISTER_COMPONENT_CODE(SC_ROMA_IPC_CHANNEL, 0x0484)
DEFINE_ERROR_CODE(SC_ROMA_IPC_CHANNEL_NO_RECORDED_CODE_OBJECT,
                  SC_ROMA_IPC_CHANNEL, 0x0001,
                  "There is no recorded code object in the IPC channel.",
                  HttpStatusCode::NOT_FOUND)
}  // namespace google::scp::core::errors
