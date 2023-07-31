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

#include "ipc_manager.h"

#include <memory>

#include "error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using std::make_unique;

namespace google::scp::roma::ipc {

IpcManager* IpcManager::instance_ = nullptr;
RoleId IpcManager::my_process_role_;
thread_local RoleId IpcManager::my_thread_role_;

IpcManager::IpcManager(size_t num_processes) : num_processes_(num_processes) {}

ExecutionResult IpcManager::Init() noexcept {
  // create all shared memory segments
  shared_mem_.reserve(num_processes_);
  for (size_t i = 0; i < num_processes_; ++i) {
    auto& shared_mem = shared_mem_.emplace_back();
    auto result = shared_mem.Create(kSharedMemorySegmentSize);
    if (!result) {
      return result;
    }
    // Construct an IpcChannel right on the shared memory segment.
    auto* ipc_channel_ptr = new (shared_mem.Get()) IpcChannel(shared_mem);
    // Construct the unique_ptr
    auto& ipc_channel = ipc_channels_.emplace_back(ipc_channel_ptr);
    ipc_channel->Init();
  }
  return SuccessExecutionResult();
}

ExecutionResult IpcManager::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult IpcManager::Stop() noexcept {
  // Destruct each IpcChannel first. Note that since we used NoOpDelete in the
  // unique_ptr, no deallocation is done here, on purpose.
  ipc_channels_.clear();
  // Then dispose the memory by unmap.
  for (auto& m : shared_mem_) {
    auto ret = m.Unmap();
    if (!ret) {
      return ret;
    }
  }
  shared_mem_.clear();
  return SuccessExecutionResult();
}

ExecutionResult IpcManager::SetUpIpcForMyProcess(RoleId role) {
  if (role.Bad() || role.IsDispatcher()) {
    return FailureExecutionResult(
        core::errors::SC_ROMA_IPC_MANAGER_BAD_WORKER_ROLE);
  }
  if (role.GetId() >= num_processes_) {
    return FailureExecutionResult(
        core::errors::SC_ROMA_IPC_MANAGER_INVALID_INDEX);
  }
  my_process_role_ = role;
  my_thread_role_ = role;
  for (uint32_t i = 0; i < num_processes_; ++i) {
    if (i == role.GetId()) {
      continue;
    }
    auto ret = shared_mem_[i].Unmap();
    if (!ret) {
      return ret;
    }
  }
  ipc_channels_[role.GetId()]->GetMemPool().SetThisThreadMemPool();
  return SuccessExecutionResult();
}

ExecutionResult IpcManager::SetUpIpcForMyThread(RoleId role) {
  if (role.Bad() || !role.IsDispatcher()) {
    return FailureExecutionResult(
        core::errors::SC_ROMA_IPC_MANAGER_BAD_DISPATCHER_ROLE);
  }
  if (role.GetId() >= num_processes_) {
    return FailureExecutionResult(
        core::errors::SC_ROMA_IPC_MANAGER_INVALID_INDEX);
  }
  my_thread_role_ = role;
  ipc_channels_[role.GetId()]->GetMemPool().SetThisThreadMemPool();
  return SuccessExecutionResult();
}

IpcChannel& IpcManager::GetIpcChannel() {
  // If thread role is ok to use, use it; otherwise use process role.
  if (!my_thread_role_.Bad()) {
    return *ipc_channels_[my_thread_role_.GetId()];
  }
  return *ipc_channels_[my_process_role_.GetId()];
}

void IpcManager::ReleaseLocks() {
  for (auto& ipc_channel : ipc_channels_) {
    ipc_channel->ReleaseLocks();
  }
}

}  // namespace google::scp::roma::ipc
