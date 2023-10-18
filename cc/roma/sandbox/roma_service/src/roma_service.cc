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

#include "roma_service.h"

#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "roma/logging/src/logging.h"
#include "roma/sandbox/worker_api/src/worker_api_sapi.h"
#include "roma/sandbox/worker_pool/src/worker_pool_api_sapi.h"

#include "error_codes.h"

using google::scp::core::AsyncExecutor;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_ROMA_SERVICE_COULD_NOT_CREATE_FD_PAIR;
using google::scp::roma::sandbox::dispatcher::Dispatcher;
using google::scp::roma::sandbox::native_function_binding::
    NativeFunctionHandlerSapiIpc;
using google::scp::roma::sandbox::native_function_binding::NativeFunctionTable;
using google::scp::roma::sandbox::worker_api::WorkerApiSapi;
using google::scp::roma::sandbox::worker_api::WorkerApiSapiConfig;
using google::scp::roma::sandbox::worker_pool::WorkerPoolApiSapi;
using std::function;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::thread;
using std::vector;

namespace google::scp::roma::sandbox::roma_service {
RomaService* RomaService::instance_ = nullptr;

ExecutionResult RomaService::Init() noexcept {
  size_t concurrency = config_.number_of_workers;
  if (concurrency == 0) {
    concurrency = thread::hardware_concurrency();
  }

  size_t worker_queue_cap = config_.worker_queue_max_items;
  if (worker_queue_cap == 0) {
    worker_queue_cap = 100;
  }

  auto native_function_binding_info_or =
      SetupNativeFunctionHandler(concurrency);
  RETURN_IF_FAILURE(native_function_binding_info_or.result());

  auto native_function_binding_info = *native_function_binding_info_or;
  auto result = SetupWorkers(native_function_binding_info);
  RETURN_IF_FAILURE(result);

  async_executor_ = make_shared<AsyncExecutor>(concurrency, worker_queue_cap);
  result = async_executor_->Init();
  RETURN_IF_FAILURE(result);

  // TODO: Make max_pending_requests configurable
  dispatcher_ = make_shared<class Dispatcher>(
      async_executor_, worker_pool_,
      concurrency * worker_queue_cap /*max_pending_requests*/,
      config_.code_version_cache_size);
  result = dispatcher_->Init();
  RETURN_IF_FAILURE(result);
  ROMA_VLOG(1) << "RomaService Init with " << config_.number_of_workers
               << " workers. The capacity of code cache is "
               << config_.code_version_cache_size;
  return SuccessExecutionResult();
}

ExecutionResult RomaService::Run() noexcept {
  auto result = native_function_binding_handler_->Run();
  RETURN_IF_FAILURE(result);

  result = async_executor_->Run();
  RETURN_IF_FAILURE(result);

  result = worker_pool_->Run();
  RETURN_IF_FAILURE(result);

  result = dispatcher_->Run();
  RETURN_IF_FAILURE(result);

  return SuccessExecutionResult();
}

ExecutionResult RomaService::Stop() noexcept {
  if (native_function_binding_handler_) {
    auto result = native_function_binding_handler_->Stop();
    RETURN_IF_FAILURE(result);
  }

  if (dispatcher_) {
    auto result = dispatcher_->Stop();
    RETURN_IF_FAILURE(result);
  }

  if (worker_pool_) {
    auto result = worker_pool_->Stop();
    RETURN_IF_FAILURE(result);
  }

  if (async_executor_) {
    auto result = async_executor_->Stop();
    RETURN_IF_FAILURE(result);
  }

  return SuccessExecutionResult();
}

ExecutionResultOr<RomaService::NativeFunctionBindingSetup>
RomaService::SetupNativeFunctionHandler(size_t concurrency) {
  native_function_binding_table_ = make_shared<NativeFunctionTable>();

  vector<shared_ptr<FunctionBindingObjectV2>> function_bindings;
  config_.GetFunctionBindings(function_bindings);

  vector<string> function_names;

  for (auto& binding : function_bindings) {
    auto result = native_function_binding_table_->Register(
        binding->function_name, binding->function);
    RETURN_IF_FAILURE(result);

    function_names.push_back(binding->function_name);
  }

  vector<int> local_fds;
  vector<int> remote_fds;

  for (int i = 0; i < concurrency; i++) {
    int fd_pair[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_pair) != 0) {
      return FailureExecutionResult(SC_ROMA_SERVICE_COULD_NOT_CREATE_FD_PAIR);
    }

    local_fds.push_back(fd_pair[0]);
    remote_fds.push_back(fd_pair[1]);
  }

  native_function_binding_handler_ = make_shared<NativeFunctionHandlerSapiIpc>(
      native_function_binding_table_, local_fds, remote_fds);
  auto result = native_function_binding_handler_->Init();
  RETURN_IF_FAILURE(result);

  NativeFunctionBindingSetup setup{
      .remote_file_descriptors = remote_fds,
      .local_file_descriptors = local_fds,
      .js_function_names = function_names,
  };

  return setup;
}

ExecutionResult RomaService::SetupWorkers(
    const NativeFunctionBindingSetup& native_binding_setup) {
  vector<WorkerApiSapiConfig> worker_configs;
  auto remote_fds = native_binding_setup.remote_file_descriptors;
  auto function_names = native_binding_setup.js_function_names;

  auto concurrency = remote_fds.size();

  JsEngineResourceConstraints resource_constraints;
  config_.GetJsEngineResourceConstraints(resource_constraints);

  for (int i = 0; i < concurrency; i++) {
    WorkerApiSapiConfig worker_api_sapi_config{
        .worker_js_engine = worker::WorkerFactory::WorkerEngine::v8,
        .js_engine_require_code_preload = true,
        .compilation_context_cache_size = config_.code_version_cache_size,
        .native_js_function_comms_fd = remote_fds.at(i),
        .native_js_function_names = function_names,
        .max_worker_virtual_memory_mb = config_.max_worker_virtual_memory_mb,
        .js_engine_resource_constraints = resource_constraints,
        .js_engine_max_wasm_memory_number_of_pages =
            config_.max_wasm_memory_number_of_pages};

    worker_configs.push_back(worker_api_sapi_config);
  }

  worker_pool_ = make_shared<WorkerPoolApiSapi>(worker_configs, concurrency);
  return worker_pool_->Init();
}
}  // namespace google::scp::roma::sandbox::roma_service
