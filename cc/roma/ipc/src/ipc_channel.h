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

#include "roma/common/src/shared_memory.h"
#include "roma/common/src/shared_memory_pool.h"
#include "roma/common/src/shm_allocator.h"
#include "roma/common/src/shm_mutex.h"
#include "roma/interface/ipc_channel_interface.h"

#include "ipc_message.h"
#include "work_container.h"

namespace google::scp::roma::ipc {
using common::SharedMemoryPool;
using common::SharedMemorySegment;
using common::ShmAllocator;

/**
 * IpcChannel is designed to stay at top (beginning) of a shared memory segment,
 * and is in charge of IPC APIs between the dispatcher and the workers. The rest
 * of the memory space in the segment will be used as memory pool storage. This
 * ideally is the sole accessibility interface of a shared memory segment, a
 * higher abstraction of the IPC over shared memory. It works like a
 * bi-directional socket that both ends can read and write.
 */
class IpcChannel : public IpcChannelInterface<Request, Response> {
 public:
  explicit IpcChannel(SharedMemorySegment& shared_memory);

  /// Create a mempool allocator for type T.
  template <typename T>
  ShmAllocator<T> GetAllocator() {
    return ShmAllocator<T>(mem_pool_);
  }

  /// Get the memory pool of this IpcChannel.
  SharedMemoryPool& GetMemPool() { return mem_pool_; }

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult TryAcquirePushRequest() override;

  core::ExecutionResult PushRequest(std::unique_ptr<Request> request) override;
  core::ExecutionResult PopRequest(Request*& request) override;

  core::ExecutionResult PushResponse(
      std::unique_ptr<Response> response) override;
  core::ExecutionResult PopResponse(
      std::unique_ptr<Response>& response) override;

  void ReleaseLocks();

  /**
   * @brief Release the lock that is used to guard popping requests from this
   * IPC channel to allow popping the request at the top.
   */
  void ReleasePopRequestLock();

  /**
   * @brief Whether the IPC channel has at least one request pending to be
   * worked.
   *
   * @return true
   * @return false
   */
  bool HasPendingRequest();

  /// @brief Get the last code object that was recorded in this channel. Note
  /// that the code object will not include the inputs.
  /// @param[out] code_obj The output code object.
  /// @return Success if there is a code object.
  core::ExecutionResult GetLastRecordedCodeObjectWithoutInputs(
      std::unique_ptr<RomaCodeObj>& code_obj);

 private:
  /// @brief Record the last valid code object.
  /// @param request The request that was popped from the IPC channel.
  void RecordLastCodeObject(Request*& request) noexcept;

  SharedMemorySegment& shared_memory_;
  SharedMemoryPool mem_pool_;
  std::unique_ptr<WorkContainer> work_container_;
  // The last code object item contained in the request that was popped from
  // this channel. Note that this code object will NOT include the inputs.
  std::unique_ptr<RomaCodeObj> last_code_object_without_inputs_;

  // Set when a request is read from the work container and cleared once a
  // response is created for the request.
  bool pending_request_;
};  // class IpcChannel
}  // namespace google::scp::roma::ipc
