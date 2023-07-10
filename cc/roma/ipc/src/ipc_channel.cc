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

#include "ipc_channel.h"

#include <chrono>
#include <memory>
#include <utility>

#include "error_codes.h"

namespace google::scp::roma::ipc {

using core::ExecutionResult;
using core::FailureExecutionResult;
using core::SuccessExecutionResult;
using core::errors::SC_ROMA_IPC_CHANNEL_NO_RECORDED_CODE_OBJECT;
using std::make_unique;
using std::unique_ptr;

IpcChannel::IpcChannel(SharedMemorySegment& shared_memory)
    : shared_memory_(shared_memory),
      mem_pool_(),
      last_code_object_without_inputs_(nullptr),
      pending_request_(false) {}

ExecutionResult IpcChannel::Init() noexcept {
  // Initialize the memory pool first and then the rest.
  uintptr_t pool_location = reinterpret_cast<uintptr_t>(shared_memory_.Get());
  pool_location += sizeof(*this);
  if (shared_memory_.Size() < sizeof(*this)) {
    // TODO return error
  }
  size_t pool_size = shared_memory_.Size() - sizeof(*this);
  mem_pool_.Init(reinterpret_cast<void*>(pool_location), pool_size);
  auto ctx = SharedMemoryPool::SwitchTo(mem_pool_);
  work_container_ = std::make_unique<WorkContainer>();

  return SuccessExecutionResult();
}

ExecutionResult IpcChannel::Stop() noexcept {
  auto ctx = SharedMemoryPool::SwitchTo(mem_pool_);
  work_container_.reset();

  return SuccessExecutionResult();
}

ExecutionResult IpcChannel::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult IpcChannel::TryAcquirePushRequest() {
  return work_container_->TryAcquireAdd();
}

ExecutionResult IpcChannel::PushRequest(unique_ptr<Request> request) {
  auto item = make_unique<WorkItem>();
  item->request = move(request);
  return work_container_->Add(move(item));
}

void IpcChannel::RecordLastCodeObject(Request*& request) noexcept {
  // Conditions to store a code object:
  // * The code object has code in it (not Empty())
  // AND
  // * The request is an Update request
  // AND
  // * The last stored code object is nullptr
  //   OR
  //   The version number of the new code object is larger than the version
  //   number of the stored one.
  if (request->type == RequestType::kUpdate && !request->code_obj->Empty() &&
      (!last_code_object_without_inputs_ ||
       request->code_obj->version_num >
           last_code_object_without_inputs_->version_num)) {
    // Copy the code object. RequestType::kUpdate code objects do not include
    // inputs.
    auto popped_code_object = make_unique<RomaCodeObj>(*request->code_obj);
    last_code_object_without_inputs_.swap(popped_code_object);
  }
}

ExecutionResult IpcChannel::PopRequest(Request*& request) {
  auto result = work_container_->GetRequest(request);

  if (result.Successful()) {
    // Keep track of the last code object that was popped from this IPC channel.
    RecordLastCodeObject(request);
    pending_request_ = true;
  }

  return result;
}

ExecutionResult IpcChannel::PushResponse(unique_ptr<Response> response) {
  auto result = work_container_->CompleteRequest(move(response));

  if (result.Successful()) {
    pending_request_ = false;
  }

  return result;
}

ExecutionResult IpcChannel::PopResponse(unique_ptr<Response>& response) {
  unique_ptr<WorkItem> item;
  auto result = work_container_->GetCompleted(item);
  if (!result.Successful()) {
    return result;
  }
  response = move(item->response);
  response->request = move(item->request);
  return SuccessExecutionResult();
}

void IpcChannel::ReleaseLocks() {
  work_container_->ReleaseLocks();
}

void IpcChannel::ReleasePopRequestLock() {
  work_container_->ReleaseGetRequestLock();
}

bool IpcChannel::HasPendingRequest() {
  return pending_request_;
}

ExecutionResult IpcChannel::GetLastRecordedCodeObjectWithoutInputs(
    unique_ptr<RomaCodeObj>& code_obj) {
  if (!last_code_object_without_inputs_) {
    return FailureExecutionResult(SC_ROMA_IPC_CHANNEL_NO_RECORDED_CODE_OBJECT);
  }

  code_obj = make_unique<RomaCodeObj>(*last_code_object_without_inputs_);
  return SuccessExecutionResult();
}
}  // namespace google::scp::roma::ipc
