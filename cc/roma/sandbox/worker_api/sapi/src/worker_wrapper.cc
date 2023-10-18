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

#include "worker_wrapper.h"

#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "core/common/time_provider/src/stopwatch.h"
#include "core/interface/errors.h"
#include "roma/config/src/config.h"
#include "roma/logging/src/logging.h"
#include "roma/sandbox/constants/constants.h"
#include "roma/sandbox/worker_factory/src/worker_factory.h"

#include "error_codes.h"

using absl::flat_hash_map;
using absl::MakeConstSpan;
using absl::Span;
using absl::string_view;
using google::scp::core::StatusCode;
using google::scp::core::common::Stopwatch;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_DESERIALIZE_INIT_DATA;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_DESERIALIZE_RUN_CODE_DATA;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_SERIALIZE_RUN_CODE_RESPONSE_DATA;
using google::scp::core::errors::SC_ROMA_WORKER_API_UNINITIALIZED_WORKER;
using google::scp::roma::JsEngineResourceConstraints;
using google::scp::roma::sandbox::constants::kExecutionMetricJsEngineCallNs;
using google::scp::roma::sandbox::worker::Worker;
using google::scp::roma::sandbox::worker::WorkerFactory;
using std::shared_ptr;
using std::string;
using std::vector;

namespace {

shared_ptr<Worker> worker_;

StatusCode Init(worker_api::WorkerInitParamsProto* init_params) {
  if (worker_) {
    Stop();
  }

  auto worker_engine = static_cast<WorkerFactory::WorkerEngine>(
      init_params->worker_factory_js_engine());

  WorkerFactory::FactoryParams factory_params;
  factory_params.engine = worker_engine;
  factory_params.require_preload =
      init_params->require_code_preload_for_execution();
  factory_params.compilation_context_cache_size =
      init_params->compilation_context_cache_size();

  if (worker_engine == WorkerFactory::WorkerEngine::v8) {
    vector<string> native_js_function_names(
        init_params->native_js_function_names().begin(),
        init_params->native_js_function_names().end());

    JsEngineResourceConstraints resource_constraints;
    resource_constraints.initial_heap_size_in_mb =
        static_cast<size_t>(init_params->js_engine_initial_heap_size_mb());
    resource_constraints.maximum_heap_size_in_mb =
        static_cast<size_t>(init_params->js_engine_maximum_heap_size_mb());

    WorkerFactory::V8WorkerEngineParams v8_params{
        .native_js_function_comms_fd =
            init_params->native_js_function_comms_fd(),
        .native_js_function_names = native_js_function_names,
        .resource_constraints = resource_constraints,
        .max_wasm_memory_number_of_pages = static_cast<size_t>(
            init_params->js_engine_max_wasm_memory_number_of_pages())};

    factory_params.v8_worker_engine_params = v8_params;
  }

  auto worker_or = WorkerFactory::Create(factory_params);
  if (!worker_or.result().Successful()) {
    return worker_or.result().status_code;
  }

  worker_ = *worker_or;
  ROMA_VLOG(1) << "Worker wrapper successfully created the worker";
  return worker_->Init().status_code;
}

StatusCode RunCode(worker_api::WorkerParamsProto* params) {
  if (!worker_) {
    return SC_ROMA_WORKER_API_UNINITIALIZED_WORKER;
  }

  const auto& code = params->code();
  vector<string_view> input;
  for (int i = 0; i < params->input_size(); i++) {
    input.push_back(params->input().at(i));
  }
  flat_hash_map<string, string> metadata;
  for (auto&& element : params->metadata()) {
    metadata[element.first] = element.second;
  }
  auto wasm_bin = reinterpret_cast<const uint8_t*>(params->wasm().c_str());
  Span<const uint8_t> wasm = MakeConstSpan(wasm_bin, params->wasm().length());

  Stopwatch stopwatch;
  stopwatch.Start();
  auto response_or = worker_->RunCode(code, input, metadata, wasm);
  auto run_code_elapsed_ns = stopwatch.Stop();
  (*params->mutable_metrics())[kExecutionMetricJsEngineCallNs] =
      run_code_elapsed_ns.count();

  if (!response_or.result().Successful()) {
    return response_or.result().status_code;
  }

  for (const auto& pair : response_or.value().metrics) {
    (*params->mutable_metrics())[pair.first] = pair.second;
  }

  params->set_response(*response_or.value().response);
  return SC_OK;
}

}  // namespace

StatusCode InitFromSerializedData(sapi::LenValStruct* data) {
  worker_api::WorkerInitParamsProto init_params;
  if (!init_params.ParseFromArray(data->data, data->size)) {
    return SC_ROMA_WORKER_API_COULD_NOT_DESERIALIZE_INIT_DATA;
  }

  ROMA_VLOG(1) << "Worker wrapper successfully received the init data";
  return Init(&init_params);
}

StatusCode Run() {
  if (!worker_) {
    return SC_ROMA_WORKER_API_UNINITIALIZED_WORKER;
  }

  return worker_->Run().status_code;
}

StatusCode Stop() {
  if (!worker_) {
    return SC_ROMA_WORKER_API_UNINITIALIZED_WORKER;
  }
  auto result = worker_->Stop();
  worker_.reset();
  return result.status_code;
}

StatusCode RunCodeFromSerializedData(sapi::LenValStruct* data) {
  worker_api::WorkerParamsProto params;
  if (!params.ParseFromArray(data->data, data->size)) {
    return SC_ROMA_WORKER_API_COULD_NOT_DESERIALIZE_RUN_CODE_DATA;
  }

  ROMA_VLOG(1) << "Worker wrapper successfully received the request data";
  auto result = RunCode(&params);
  if (result != SC_OK) {
    return result;
  }

  // Don't return the input or code
  params.clear_code();
  params.clear_input();

  int serialized_size = params.ByteSizeLong();
  uint8_t* serialized_data = static_cast<uint8_t*>(malloc(serialized_size));
  if (!serialized_data) {
    return SC_ROMA_WORKER_API_COULD_NOT_SERIALIZE_RUN_CODE_RESPONSE_DATA;
  }

  if (!params.SerializeToArray(serialized_data, serialized_size)) {
    // In this failure scenario, free the buffer.
    // We don't free it otherwise, as it is free'd in the destructor of the
    // LenValStruct* passed to this function, which is owned by SAPI.
    free(serialized_data);
    return SC_ROMA_WORKER_API_COULD_NOT_SERIALIZE_RUN_CODE_RESPONSE_DATA;
  }

  // Free old data
  free(data->data);

  data->data = serialized_data;
  data->size = serialized_size;

  ROMA_VLOG(1) << "Worker wrapper successfully executed the request";
  return result;
}
