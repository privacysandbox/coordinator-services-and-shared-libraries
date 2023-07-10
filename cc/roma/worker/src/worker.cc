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

#include "worker.h"

#include <memory>
#include <string>
#include <utility>

#include "core/common/time_provider/src/time_provider.h"
#include "include/v8.h"
#include "roma/common/src/process.h"
#include "roma/interface/roma.h"
#include "roma/ipc/src/ipc_message.h"

#include "error_codes.h"
#include "execution_manager.h"
#include "execution_utils.h"
#include "execution_watchdog.h"

using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::TimeProvider;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::SC_ROMA_V8_WORKER_ITEM_WORKED_ON_BEFORE;
using google::scp::roma::common::Process;
using google::scp::roma::common::RoleId;
using google::scp::roma::common::RomaString;
using google::scp::roma::ipc::IpcManager;
using google::scp::roma::ipc::Request;
using google::scp::roma::ipc::RequestType;
using google::scp::roma::ipc::RomaCodeResponse;
using google::scp::roma::worker::ExecutionManager;
using google::scp::roma::worker::ExecutionUtils;
using std::atomic;
using std::make_unique;
using std::move;
using std::shared_ptr;
using std::strcmp;
using std::string;
using std::unique_ptr;
using std::vector;

namespace google::scp::roma::worker {
Worker::Worker(
    RoleId role_id,
    const vector<shared_ptr<FunctionBindingObjectBase>>& function_bindings)
    : ipc_channel_(IpcManager::Instance()->GetIpcChannel(role_id)),
      function_bindings_(function_bindings) {
  auto* ptr = ipc_channel_.GetMemPool().Allocate(1);
  stop_called_ = new (ptr) atomic<bool>(false);
  ptr = ipc_channel_.GetMemPool().Allocate(sizeof(atomic<pid_t>));
  worker_process_id_ = new (ptr) atomic<pid_t>(-1);

  // Must add pointers that are not within the v8 heap to external_references_
  // so that the snapshot serialization works.
  external_references_.push_back(
      reinterpret_cast<intptr_t>(ExecutionManager::GlobalV8FunctionCallback));
  for (auto& func : function_bindings) {
    external_references_.push_back(reinterpret_cast<intptr_t>(func.get()));
  }
  // Must be null terminated
  external_references_.push_back(0);
}

ExecutionResult Worker::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult Worker::Run() noexcept {
  worker_process_id_->store(getpid());
  ExecutionResult result;

  {
    unique_ptr<ipc::RomaCodeObj> code_obj_ptr;
    result = ipc_channel_.GetLastRecordedCodeObjectWithoutInputs(code_obj_ptr);
    // If there is a recorded code object in this worker's IPC channel, update
    // the worker's code with it.
    if (result.Successful()) {
      auto code_obj = *code_obj_ptr;
      // NOTE: This update could fail if the stored code object contains code
      // that does not compile, so we don't check the execution result since
      // this would be expected.
      Update(code_obj);
    }
  }

  if (ipc_channel_.HasPendingRequest()) {
    // This means that the worker died while handling a request. This
    // request is most likely the cause of the worker's death, so we
    // need to make sure we can pick up that request again and reply to
    // it with a failure. To allow this, we must unlock the request
    // acquisition lock.
    ipc_channel_.ReleasePopRequestLock();
  }

  while (!stop_called_->load()) {
    Request* request;
    // PopRequest is a blocking call, and will always return Success when
    // there are items to work on. However, when stopping, it will return a
    // failure. In which case we just want to continue, to pick up the
    // stop_called_ flag on this main loop.
    if (!ipc_channel_.PopRequest(request).Successful()) {
      continue;
    }

    {
      // If this request had been worked on before, then this means that this
      // worker took this request but died in the middle of processing it. So
      // we will just reply to it with a failure.
      if (request->has_been_worked) {
        result = GenerateRequestResponse(
            request, "",
            core::FailureExecutionResult(
                SC_ROMA_V8_WORKER_ITEM_WORKED_ON_BEFORE));
        if (!result.Successful()) {
          // TODO: Log or report metric
        }
        continue;
      }
      // Mark that this request has already been worked on.
      request->has_been_worked = true;
    }

    if (request->type == RequestType::kUpdate) {
      result = Update(request);
      if (!result.Successful()) {
        // TODO: Log or report metric
      }
    }

    if (request->type == RequestType::kExecute) {
      result = Execute(request);
      if (!result.Successful()) {
        // TODO: Log or report metric
      }
    }
  }

  ipc_channel_.GetMemPool().Deallocate(stop_called_);
  ipc_channel_.GetMemPool().Deallocate(worker_process_id_);

  return SuccessExecutionResult();
}

ExecutionResult Worker::Stop() noexcept {
  stop_called_->store(true);

  execution_manager_.Stop();
  return SuccessExecutionResult();
}

ExecutionResult Worker::Update(Request* request) noexcept {
  auto& code_obj = *request->code_obj;

  RomaString err_msg;
  auto result = execution_manager_.Create(code_obj, err_msg, function_bindings_,
                                          external_references_.data());
  return GenerateRequestResponse(request, "", result);
}

ExecutionResult Worker::Update(const ipc::RomaCodeObj& code_object) noexcept {
  RomaString err_msg;
  auto result = execution_manager_.Create(
      code_object, err_msg, function_bindings_, external_references_.data());
  if (!result.Successful()) {
    return result;
  }

  return SuccessExecutionResult();
}

ExecutionResult Worker::Execute(Request* request) noexcept {
  auto& code_obj = *request->code_obj;
  RomaString output;
  RomaString err_msg;
  auto result = execution_manager_.Process(code_obj, output, err_msg);
  return GenerateRequestResponse(request, output, result);
}

ExecutionResult Worker::GenerateRequestResponse(Request*& request,
                                                const ipc::RomaString& output,
                                                const ExecutionResult& result) {
  auto roma_response = make_unique<RomaCodeResponse>();
  roma_response->id = request->code_obj->id;
  roma_response->resp = output;
  auto ipc_response = make_unique<ipc::Response>();
  ipc_response->result = result;
  ipc_response->response = move(roma_response);
  return ipc_channel_.PushResponse(move(ipc_response));
}
}  // namespace google::scp::roma::worker
