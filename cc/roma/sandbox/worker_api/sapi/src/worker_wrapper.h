/*
 * Copyright 2023 Google LLC
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

// This is a wrapper to be run within the sandboxed API (sapi).
// This wrapper is used to interact with the worker from outside the sandbox,
// but these are the actual functions that the sandbox infra calls.
// The extern "C" is to avoid name mangling to enable sapi code generation.

#pragma once

#include "public/core/interface/execution_result.h"
#include "roma/sandbox/worker_api/sapi/src/worker_init_params.pb.h"
#include "roma/sandbox/worker_api/sapi/src/worker_params.pb.h"
#include "sandboxed_api/lenval_core.h"

#include "error_codes.h"

extern "C" google::scp::core::StatusCode InitFromSerializedData(
    sapi::LenValStruct* data);

extern "C" google::scp::core::StatusCode Run();

extern "C" google::scp::core::StatusCode Stop();
extern "C" google::scp::core::StatusCode RunCodeFromSerializedData(
    sapi::LenValStruct* data);
