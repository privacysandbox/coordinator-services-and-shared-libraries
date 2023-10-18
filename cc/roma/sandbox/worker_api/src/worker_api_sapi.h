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
#include <mutex>
#include <string>
#include <vector>

#include "roma/config/src/config.h"
#include "roma/sandbox/worker_api/sapi/src/worker_sandbox_api.h"
#include "roma/sandbox/worker_factory/src/worker_factory.h"

#include "worker_api.h"

namespace google::scp::roma::sandbox::worker_api {

struct WorkerApiSapiConfig {
  worker::WorkerFactory::WorkerEngine worker_js_engine;
  bool js_engine_require_code_preload;
  size_t compilation_context_cache_size;
  int native_js_function_comms_fd;
  std::vector<std::string> native_js_function_names;
  size_t max_worker_virtual_memory_mb;
  JsEngineResourceConstraints js_engine_resource_constraints;
  size_t js_engine_max_wasm_memory_number_of_pages;
};

class WorkerApiSapi : public WorkerApi {
 public:
  explicit WorkerApiSapi(const WorkerApiSapiConfig& config) {
    sandbox_api_ = std::make_unique<WorkerSandboxApi>(
        config.worker_js_engine, config.js_engine_require_code_preload,
        config.compilation_context_cache_size,
        config.native_js_function_comms_fd, config.native_js_function_names,
        config.max_worker_virtual_memory_mb,
        config.js_engine_resource_constraints.initial_heap_size_in_mb,
        config.js_engine_resource_constraints.maximum_heap_size_in_mb,
        config.js_engine_max_wasm_memory_number_of_pages);
  }

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResultOr<WorkerApi::RunCodeResponse> RunCode(
      const WorkerApi::RunCodeRequest& request) noexcept override;

  core::ExecutionResult Terminate() noexcept override;

 private:
  std::unique_ptr<WorkerSandboxApi> sandbox_api_;
  std::mutex run_code_mutex_;
};
}  // namespace google::scp::roma::sandbox::worker_api
