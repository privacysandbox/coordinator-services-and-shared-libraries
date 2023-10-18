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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "public/core/interface/execution_result.h"
#include "roma/config/src/config.h"
#include "roma/sandbox/js_engine/src/v8_engine/v8_js_engine.h"
#include "roma/sandbox/native_function_binding/src/native_function_invoker_sapi_ipc.h"
#include "roma/sandbox/worker/src/worker.h"

#include "error_codes.h"

namespace google::scp::roma::sandbox::worker {
class WorkerFactory {
 public:
  enum class WorkerEngine { v8 };

  struct V8WorkerEngineParams {
    int native_js_function_comms_fd;
    std::vector<std::string> native_js_function_names;
    JsEngineResourceConstraints resource_constraints;
    size_t max_wasm_memory_number_of_pages;
  };

  struct FactoryParams {
    WorkerEngine engine = WorkerEngine::v8;
    bool require_preload = true;
    size_t compilation_context_cache_size;

    V8WorkerEngineParams v8_worker_engine_params{};
  };

  static core::ExecutionResultOr<std::shared_ptr<Worker>> Create(
      const FactoryParams& params);
};
}  // namespace google::scp::roma::sandbox::worker
