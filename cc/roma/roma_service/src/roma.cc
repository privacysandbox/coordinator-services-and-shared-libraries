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

#include "roma/interface/roma.h"

#include <memory>
#include <utility>
#include <vector>

#include "roma/dispatcher/src/dispatcher.h"

#include "roma_service.h"

using absl::OkStatus;
using absl::Status;
using absl::StatusCode;
using google::scp::roma::roma_service::RomaService;
using std::make_unique;
using std::move;
using std::unique_ptr;
using std::vector;

namespace google::scp::roma {
namespace {
template <typename RequestT>
Status ExecutionObjectValidation(const std::string& function_name,
                                 const RequestT& invocation_req) {
  if (invocation_req->version_num == 0) {
    return Status(StatusCode::kInvalidArgument,
                  "Roma " + function_name + " failed due to invalid version.");
  }

  if (invocation_req->handler_name.empty()) {
    return Status(
        StatusCode::kInvalidArgument,
        "Roma " + function_name + " failed due to empty handler name.");
  }

  return OkStatus();
}

template <typename RequestT>
Status ExecuteInternal(unique_ptr<RequestT> invocation_req, Callback callback) {
  auto validation = ExecutionObjectValidation("Execute", invocation_req.get());
  if (!validation.ok()) {
    return validation;
  }

  auto* roma_service = RomaService::Instance();
  auto result =
      roma_service->Dispatcher().Dispatch(move(invocation_req), callback);
  if (!result.Successful()) {
    return Status(StatusCode::kInternal,
                  "Roma Execute failed due to dispatch error.");
  }
  return OkStatus();
}

template <typename RequestT>
Status BatchExecuteInternal(vector<RequestT>& batch,
                            BatchCallback batch_callback) {
  for (auto& request : batch) {
    auto validation = ExecutionObjectValidation("BatchExecute", &request);
    if (!validation.ok()) {
      return validation;
    }
  }

  auto* roma_service = RomaService::Instance();
  auto result = roma_service->Dispatcher().DispatchBatch(batch, batch_callback);
  if (!result.Successful()) {
    return Status(StatusCode::kInternal,
                  "Roma Batch Execute failed due to dispatch error.");
  }
  return OkStatus();
}
}  // namespace

Status RomaInit(const Config& config) {
  auto* roma_service = RomaService::Instance(config);
  auto result = roma_service->Init();
  if (!result.Successful()) {
    return Status(StatusCode::kInternal,
                  "Roma initialization failed due to internal error.");
  }
  result = roma_service->Run();
  if (!result.Successful()) {
    return Status(StatusCode::kInternal,
                  "Roma startup failed due to internal error.");
  }
  return OkStatus();
}

Status RomaStop() {
  auto* roma_service = RomaService::Instance();
  auto result = roma_service->Stop();
  if (!result.Successful()) {
    return Status(StatusCode::kInternal,
                  "Roma stop failed due to internal error.");
  }
  RomaService::Delete();
  return OkStatus();
}

Status Execute(unique_ptr<InvocationRequestStrInput> invocation_req,
               Callback callback) {
  return ExecuteInternal(move(invocation_req), callback);
}

Status Execute(unique_ptr<InvocationRequestSharedInput> invocation_req,
               Callback callback) {
  return ExecuteInternal(move(invocation_req), callback);
}

Status BatchExecute(vector<InvocationRequestStrInput>& batch,
                    BatchCallback batch_callback) {
  return BatchExecuteInternal(batch, batch_callback);
}

Status BatchExecute(vector<InvocationRequestSharedInput>& batch,
                    BatchCallback batch_callback) {
  return BatchExecuteInternal(batch, batch_callback);
}

Status LoadCodeObj(unique_ptr<CodeObject> code_object, Callback callback) {
  if (code_object->version_num == 0) {
    return Status(StatusCode::kInternal,
                  "Roma LoadCodeObj failed due to invalid version.");
  }
  if (code_object->js.empty() && code_object->wasm.empty()) {
    return Status(StatusCode::kInternal,
                  "Roma LoadCodeObj failed due to empty code content.");
  }
  auto* roma_service = RomaService::Instance();
  auto result =
      roma_service->Dispatcher().Broadcast(move(code_object), callback);
  if (!result.Successful()) {
    return Status(StatusCode::kInternal,
                  "Roma LoadCodeObj failed due to dispatch error.");
  }
  return OkStatus();
}

}  // namespace google::scp::roma
