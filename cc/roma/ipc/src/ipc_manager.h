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

#pragma once

#include <memory>
#include <vector>

#include "public/core/interface/execution_result.h"
#include "roma/common/src/role_id.h"
#include "roma/common/src/shared_memory.h"
#include "roma/common/src/shared_memory_pool.h"
#include "roma/common/src/shm_mutex.h"
#include "roma/interface/ipc_manager_interface.h"

#include "error_codes.h"
#include "ipc_channel.h"

namespace google::scp::roma::ipc {
using common::RoleId;

/**
 * @brief IpcManager is a top level structure for managing the Ipc resources
 * between the parent instance (dispatcher) and the children (workers). The
 * class is designed to be accessible by both parent and children via CoW
 * (copy-on-write), while children should not need to do any write access (e.g.
 * add/remove the shared memory segments) after initialization.
 */
class IpcManager : public IpcManagerInterface<Request, Response> {
 public:
  core::ExecutionResult Init() noexcept override;
  core::ExecutionResult Run() noexcept override;
  core::ExecutionResult Stop() noexcept override;

  /// A IpcChannel switcher with RAII semantics. It should only be used as stack
  /// objects.
  struct Context {
    Context(IpcManager& ipc_mgr, RoleId role)
        : old_role(my_thread_role_),
          memory_ctx(ipc_mgr.GetIpcChannel(role).GetMemPool()) {
      my_thread_role_ = role;
    }

    ~Context() { IpcManager::my_thread_role_ = old_role; }

    // Disable heap allocations, so that it can only be used on stack.
    static void* operator new(size_t) = delete;
    static void* operator new[](size_t) = delete;
    static void operator delete(void*) = delete;
    static void operator delete[](void*) = delete;

    RoleId old_role;
    SharedMemoryPool::Context memory_ctx;
  };

  /// Switch to an IpcChannel and its memory pool.
  Context SwitchTo(RoleId role) { return Context(*this, role); }

  /// Constructs an IpcManager to prepare for communicating with \a
  /// num_processes workers.
  explicit IpcManager(size_t num_processes);

  /// Get the IpcChannel for a specific role.
  IpcChannel& GetIpcChannel(RoleId role) override {
    return *ipc_channels_[role.GetId()];
  }

  IpcChannel& GetIpcChannel() override;

  /// Set up the default IPC resources to use for current thread. This shall be
  /// thread-safe as we are going to call in different threads.
  core::ExecutionResult SetUpIpcForMyThread(RoleId my_thread_role) override;

  /// Set up the default IPC resources to use for current process. This shall be
  /// called in workers first thing after fork.
  core::ExecutionResult SetUpIpcForMyProcess(common::RoleId role) override;

  /// Get the memory segment for a specific worker. This should only be called
  /// by parent (dispatcher) processes.
  SharedMemorySegment& GetSharedMemorySegment(RoleId role) {
    return shared_mem_[role.GetId()];
  }

  /// Get the number of processes this IpcManager supports.
  size_t GetNumProcesses() const { return num_processes_; }

  static IpcManager* Instance() { return instance_; }

  static IpcManager* Create(size_t num_processes) {
    instance_ = new IpcManager(num_processes);
    return instance_;
  }

  void ReleaseLocks();

 private:
  /// The size of each shared memory segment.
  static constexpr size_t kSharedMemorySegmentSize = 1024 * 1024 * 64;

  static IpcManager* instance_;
  /// The role of current (worker) process.
  static RoleId my_process_role_;
  /// The role of current (dispatcher) thread.
  static thread_local RoleId my_thread_role_;
  /// The shared memory segments. shared_mem_[i] is shared between the
  /// dispatcher and worker i.
  std::vector<SharedMemorySegment> shared_mem_;
  /// The IpcChannel we use between the dispatcher and workers. They are
  /// essentially just the manager of the corresponding SharedMemorySegment. We
  /// may simply cast the SharedMemorySegment beginning address to an
  /// IpcChannel, however IpcChannel being virtual class makes it messy.
  std::vector<std::unique_ptr<IpcChannel, common::NoOpDelete>> ipc_channels_;
  /// The total number of worker processes we intend to support.
  size_t num_processes_;
};

}  // namespace google::scp::roma::ipc
