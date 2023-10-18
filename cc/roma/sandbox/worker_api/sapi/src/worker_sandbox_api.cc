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

#include "worker_sandbox_api.h"

#include <sys/syscall.h>

#include <linux/audit.h>

#include <chrono>
#include <limits>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "roma/logging/src/logging.h"
#include "roma/sandbox/worker_api/sapi/src/worker_init_params.pb.h"
#include "roma/sandbox/worker_api/sapi/src/worker_params.pb.h"
#include "sandboxed_api/lenval_core.h"
#include "sandboxed_api/sandbox2/policy.h"
#include "sandboxed_api/sandbox2/policybuilder.h"

#include "error_codes.h"

#define ROMA_CONVERT_MB_TO_BYTES(mb) mb * 1024 * 1024

using absl::SimpleAtoi;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_ROMA_WORKER_API_COULD_NOT_CREATE_IPC_PROTO;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_DESERIALIZE_RUN_CODE_DATA;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_GET_PROTO_MESSAGE_AFTER_EXECUTION;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_INITIALIZE_SANDBOX;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_INITIALIZE_WRAPPER_API;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_RUN_CODE_THROUGH_WRAPPER_API;
using google::scp::core::errors::SC_ROMA_WORKER_API_COULD_NOT_RUN_WRAPPER_API;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_SERIALIZE_INIT_DATA;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_SERIALIZE_RUN_CODE_DATA;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_TRANSFER_FUNCTION_FD_TO_SANDBOX;
using google::scp::core::errors::SC_ROMA_WORKER_API_UNINITIALIZED_SANDBOX;
using google::scp::core::errors::SC_ROMA_WORKER_API_WORKER_CRASHED;
using std::move;
using std::numeric_limits;
using std::string;
using std::vector;
using std::this_thread::yield;

namespace google::scp::roma::sandbox::worker_api {

void WorkerSandboxApi::CreateWorkerSapiSandbox() noexcept {
  // Get the environment variable ROMA_VLOG_LEVEL value.
  int external_verbose_level = logging::GetVlogVerboseLevel();

  worker_sapi_sandbox_ = std::make_unique<WorkerSapiSandbox>(
      ROMA_CONVERT_MB_TO_BYTES(max_worker_virtual_memory_mb_),
      external_verbose_level);
}

ExecutionResult WorkerSandboxApi::Init() noexcept {
  if (native_js_function_comms_fd_ != kBadFd) {
    sapi_native_js_function_comms_fd_ =
        std::make_unique<::sapi::v::Fd>(native_js_function_comms_fd_);
    // We don't want the SAPI FD object to manage the local FD or it'd close it
    // upon its deletion.
    sapi_native_js_function_comms_fd_->OwnLocalFd(false);
  }

  if (worker_sapi_sandbox_) {
    worker_sapi_sandbox_->Terminate();
    // Wait for the sandbox to become INACTIVE
    while (worker_sapi_sandbox_->is_active()) {
      yield();
    }

    ROMA_VLOG(1) << "Successfully terminated the existing sapi sandbox";
  }

  CreateWorkerSapiSandbox();

  auto status = worker_sapi_sandbox_->Init();
  if (!status.ok()) {
    return FailureExecutionResult(
        SC_ROMA_WORKER_API_COULD_NOT_INITIALIZE_SANDBOX);
  }

  worker_wrapper_api_ =
      std::make_unique<WorkerWrapperApi>(worker_sapi_sandbox_.get());

  // Wait for the sandbox to become ACTIVE
  while (!worker_sapi_sandbox_->is_active()) {
    yield();
  }
  ROMA_VLOG(1) << "the sapi sandbox is active";

  int remote_fd = kBadFd;

  if (sapi_native_js_function_comms_fd_) {
    auto transferred = worker_sapi_sandbox_->TransferToSandboxee(
        sapi_native_js_function_comms_fd_.get());
    if (!transferred.ok()) {
      return FailureExecutionResult(
          SC_ROMA_WORKER_API_COULD_NOT_TRANSFER_FUNCTION_FD_TO_SANDBOX);
    }

    // This is to support recreating the SAPI FD object upon restarts, otherwise
    // destroying the object will try to close a non-existent file. And it has
    // to be done after the call to TransferToSandboxee.
    sapi_native_js_function_comms_fd_->OwnRemoteFd(false);

    remote_fd = sapi_native_js_function_comms_fd_->GetRemoteFd();

    ROMA_VLOG(2) << "successfully set up the remote_fd " << remote_fd
                 << " and local_fd "
                 << sapi_native_js_function_comms_fd_->GetValue()
                 << " for native function invocation from the sapi sandbox";
  }

  ::worker_api::WorkerInitParamsProto worker_init_params;
  worker_init_params.set_worker_factory_js_engine(
      static_cast<int>(worker_engine_));
  worker_init_params.set_require_code_preload_for_execution(require_preload_);
  worker_init_params.set_compilation_context_cache_size(
      compilation_context_cache_size_);
  worker_init_params.set_native_js_function_comms_fd(remote_fd);
  worker_init_params.mutable_native_js_function_names()->Assign(
      native_js_function_names_.begin(), native_js_function_names_.end());
  worker_init_params.set_js_engine_initial_heap_size_mb(
      js_engine_initial_heap_size_mb_);
  worker_init_params.set_js_engine_maximum_heap_size_mb(
      js_engine_maximum_heap_size_mb_);
  worker_init_params.set_js_engine_max_wasm_memory_number_of_pages(
      js_engine_max_wasm_memory_number_of_pages_);

  const auto serialized_size = worker_init_params.ByteSizeLong();
  vector<char> serialized_data(serialized_size);
  if (!worker_init_params.SerializeToArray(serialized_data.data(),
                                           serialized_size)) {
    LOG(ERROR) << "Failed to serialize init data.";
    return FailureExecutionResult(
        SC_ROMA_WORKER_API_COULD_NOT_SERIALIZE_INIT_DATA);
  }

  sapi::v::LenVal sapi_len_val(static_cast<const char*>(serialized_data.data()),
                               serialized_size);
  const auto status_or =
      worker_wrapper_api_->InitFromSerializedData(sapi_len_val.PtrBefore());
  if (!status_or.ok()) {
    return FailureExecutionResult(
        SC_ROMA_WORKER_API_COULD_NOT_INITIALIZE_WRAPPER_API);
  }
  if (*status_or != SC_OK) {
    return FailureExecutionResult(*status_or);
  }

  ROMA_VLOG(1) << "Successfully init the worker in the sapi sandbox";
  return SuccessExecutionResult();
}

ExecutionResult WorkerSandboxApi::Run() noexcept {
  if (!worker_sapi_sandbox_ || !worker_wrapper_api_) {
    return FailureExecutionResult(SC_ROMA_WORKER_API_UNINITIALIZED_SANDBOX);
  }

  auto status_or = worker_wrapper_api_->Run();
  if (!status_or.ok()) {
    LOG(ERROR) << "Failed to run the worker via the wrapper with: "
               << status_or.status().message();
    return FailureExecutionResult(SC_ROMA_WORKER_API_COULD_NOT_RUN_WRAPPER_API);
  } else if (*status_or != SC_OK) {
    return FailureExecutionResult(*status_or);
  }

  return SuccessExecutionResult();
}

ExecutionResult WorkerSandboxApi::Stop() noexcept {
  if ((!worker_sapi_sandbox_ && !worker_wrapper_api_) ||
      (worker_sapi_sandbox_ && !worker_sapi_sandbox_->is_active())) {
    // Nothing to stop, just return
    return SuccessExecutionResult();
  }

  if (!worker_sapi_sandbox_ || !worker_wrapper_api_) {
    return FailureExecutionResult(SC_ROMA_WORKER_API_UNINITIALIZED_SANDBOX);
  }

  auto status_or = worker_wrapper_api_->Stop();
  if (!status_or.ok()) {
    LOG(ERROR) << "Failed to stop the worker via the wrapper with: "
               << status_or.status().message();
    // The worker had already died so nothing to stop
    return SuccessExecutionResult();
  } else if (*status_or != SC_OK) {
    return FailureExecutionResult(*status_or);
  }

  worker_sapi_sandbox_->Terminate();

  return SuccessExecutionResult();
}

ExecutionResult WorkerSandboxApi::InternalRunCode(
    ::worker_api::WorkerParamsProto& params) noexcept {
  int serialized_size = params.ByteSizeLong();
  vector<char> serialized_data(serialized_size);
  if (!params.SerializeToArray(serialized_data.data(), serialized_size)) {
    LOG(ERROR) << "Failed to serialize run_code data.";
    return FailureExecutionResult(
        SC_ROMA_WORKER_API_COULD_NOT_SERIALIZE_RUN_CODE_DATA);
  }

  sapi::v::LenVal sapi_len_val(static_cast<const char*>(serialized_data.data()),
                               serialized_size);

  auto ptr_both = sapi_len_val.PtrBoth();
  auto status_or = worker_wrapper_api_->RunCodeFromSerializedData(ptr_both);

  if (!status_or.ok()) {
    return RetryExecutionResult(SC_ROMA_WORKER_API_WORKER_CRASHED);
  } else if (*status_or != SC_OK) {
    return FailureExecutionResult(*status_or);
  }

  ::worker_api::WorkerParamsProto out_params;
  if (!out_params.ParseFromArray(sapi_len_val.GetData(),
                                 sapi_len_val.GetDataSize())) {
    LOG(ERROR) << "Could not deserialize run_code response from sandbox";
    return FailureExecutionResult(
        SC_ROMA_WORKER_API_COULD_NOT_DESERIALIZE_RUN_CODE_DATA);
  }

  params = move(out_params);

  return SuccessExecutionResult();
}

ExecutionResult WorkerSandboxApi::RunCode(
    ::worker_api::WorkerParamsProto& params) noexcept {
  if (!worker_sapi_sandbox_ || !worker_wrapper_api_) {
    return FailureExecutionResult(SC_ROMA_WORKER_API_UNINITIALIZED_SANDBOX);
  }

  auto run_code_result = InternalRunCode(params);

  if (!run_code_result.Successful()) {
    if (run_code_result.Retryable()) {
      // This means that the sandbox died so we need to restart it.
      auto result = Init();
      RETURN_IF_FAILURE(result);
      result = Run();
      RETURN_IF_FAILURE(result);
    }

    return run_code_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult WorkerSandboxApi::Terminate() noexcept {
  worker_sapi_sandbox_->Terminate();
  return SuccessExecutionResult();
}
}  // namespace google::scp::roma::sandbox::worker_api
