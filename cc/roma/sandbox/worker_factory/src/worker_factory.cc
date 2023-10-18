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

#include "worker_factory.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "roma/sandbox/constants/constants.h"
#include "roma/sandbox/js_engine/src/v8_engine/v8_isolate_visitor.h"
#include "roma/sandbox/js_engine/src/v8_engine/v8_isolate_visitor_function_binding.h"
#include "roma/sandbox/js_engine/src/v8_engine/v8_js_engine.h"
#include "roma/sandbox/native_function_binding/src/native_function_invoker_sapi_ipc.h"

#include "error_codes.h"

using absl::flat_hash_map;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::SC_ROMA_WORKER_FACTORY_UNKNOWN_ENGINE_TYPE;
using google::scp::roma::sandbox::constants::kJsEngineOneTimeSetupWasmPagesKey;
using google::scp::roma::sandbox::js_engine::v8_js_engine::V8IsolateVisitor;
using google::scp::roma::sandbox::js_engine::v8_js_engine::
    V8IsolateVisitorFunctionBinding;
using google::scp::roma::sandbox::js_engine::v8_js_engine::V8JsEngine;
using google::scp::roma::sandbox::native_function_binding::
    NativeFunctionInvokerSapiIpc;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::vector;

namespace google::scp::roma::sandbox::worker {
static flat_hash_map<string, string> GetEngineOneTimeSetup(
    const WorkerFactory::FactoryParams& params) {
  flat_hash_map<string, string> one_time_setup;
  one_time_setup[kJsEngineOneTimeSetupWasmPagesKey] =
      to_string(params.v8_worker_engine_params.max_wasm_memory_number_of_pages);
  return one_time_setup;
}

ExecutionResultOr<shared_ptr<Worker>> WorkerFactory::Create(
    const WorkerFactory::FactoryParams& params) {
  if (params.engine == WorkerFactory::WorkerEngine::v8) {
    auto native_function_invoker = make_shared<NativeFunctionInvokerSapiIpc>(
        params.v8_worker_engine_params.native_js_function_comms_fd);

    vector<shared_ptr<V8IsolateVisitor>> isolate_visitors = {
        make_shared<V8IsolateVisitorFunctionBinding>(
            params.v8_worker_engine_params.native_js_function_names,
            native_function_invoker)};

    auto v8_engine = make_shared<V8JsEngine>(
        isolate_visitors, params.v8_worker_engine_params.resource_constraints);

    auto one_time_setup = GetEngineOneTimeSetup(params);
    v8_engine->OneTimeSetup(one_time_setup);

    auto worker = make_shared<Worker>(v8_engine, params.require_preload,
                                      params.compilation_context_cache_size);

    return worker;
  }

  return FailureExecutionResult(SC_ROMA_WORKER_FACTORY_UNKNOWN_ENGINE_TYPE);
}
}  // namespace google::scp::roma::sandbox::worker
