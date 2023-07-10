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

#include <stdint.h>

#include <atomic>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "core/interface/service_interface.h"
#include "roma/interface/roma.h"
#include "roma/ipc/src/ipc_manager.h"

namespace google::scp::roma::dispatcher {

/**
 * @brief The dispatcher that's in charge of dispatching code objects  or
 * invocation requests to different workers.
 */
class Dispatcher : public core::ServiceInterface {
 public:
  core::ExecutionResult Init() noexcept override;
  core::ExecutionResult Run() noexcept override;
  core::ExecutionResult Stop() noexcept override;

  /// Default construct a dispatcher.
  explicit Dispatcher(ipc::IpcManager& ipc_manager)
      : ipc_manager_(ipc_manager) {}

  /// Dispatch an invocation request to workers for execution.
  template <typename RequestT>
  core::ExecutionResult Dispatch(std::unique_ptr<RequestT> invocation_request,
                                 Callback callback) noexcept {
    // Do round-robin selection of the workers.
    uint32_t worker_index = next_worker_index_.fetch_add(1);
    worker_index %= ipc_manager_.GetNumProcesses();
    // Switch to the appropriate IpcChannel and mempool
    auto ctx =
        ipc_manager_.SwitchTo(ipc::RoleId(worker_index, true /* dispatcher */));

    auto result = ipc_manager_.GetIpcChannel().TryAcquirePushRequest();

    if (result.Successful()) {
      // Only allocate the request if we got a spot on the IPC channel
      auto request =
          std::make_unique<ipc::Request>(move(invocation_request), callback);
      request->type = ipc::RequestType::kExecute;
      result = ipc_manager_.GetIpcChannel().PushRequest(move(request));
    }
    return result;
  }

  /// Dispatch a batch of invocation request to workers for execution.
  template <typename RequestT>
  core::ExecutionResult DispatchBatch(std::vector<RequestT>& batch,
                                      BatchCallback batch_callback) noexcept {
    auto batch_size = batch.size();
    auto batch_response =
        std::make_shared<std::vector<absl::StatusOr<ResponseObject>>>(
            batch_size, absl::StatusOr<ResponseObject>());
    auto finished_counter = std::make_shared<std::atomic<size_t>>(0);

    for (size_t index = 0; index < batch_size; ++index) {
      auto callback =
          [batch_response, finished_counter, batch_callback, index](
              std::unique_ptr<absl::StatusOr<ResponseObject>> obj_response) {
            batch_response->at(index) = *obj_response;
            auto finished_value = finished_counter->fetch_add(1);
            if (finished_value + 1 == batch_response->size()) {
              batch_callback(*batch_response);
            }
          };

      auto request = std::make_unique<RequestT>(batch[index]);
      auto result = Dispatcher::Dispatch(std::move(request), callback);
      if (!result.Successful()) {
        return result;
      }
    }

    return core::SuccessExecutionResult();
  }

  /**
   * @brief Broadcast code_object is dispatching the code_object to all workers,
   * which will update their persistent precompiled code objects.
   *
   * @param code_object The code object to be broadcasted.
   * @param callback Callback function to run after broadcast is done or failed.
   * @return core::ExecutionResult
   */
  core::ExecutionResult Broadcast(std::unique_ptr<CodeObject> code_object,
                                  Callback callback) noexcept;

 private:
  /// The logic inside each response poller thread.
  void ResponsePollerWorker(uint32_t worker_index) noexcept;
  /// The response poller threads.
  // TODO: We probably don't need one poller thread per worker. This may be
  // optimized.
  std::vector<std::thread> response_pollers_;
  /// The next worker index that we dispatch to.
  std::atomic<uint32_t> next_worker_index_;
  /// The IpcManager to route the messages.
  ipc::IpcManager& ipc_manager_;
  /// The response pollers shall stop if this flag becomes true.
  std::atomic<bool> stop_;
};

}  // namespace google::scp::roma::dispatcher
