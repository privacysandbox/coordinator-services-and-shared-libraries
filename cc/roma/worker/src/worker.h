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

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "core/interface/service_interface.h"
#include "include/v8.h"
#include "public/core/interface/execution_result.h"
#include "roma/ipc/src/ipc_channel.h"
#include "roma/ipc/src/ipc_manager.h"

#include "execution_manager.h"

namespace google::scp::roma::worker {
/// @brief Worker is a single-process worker responsible for processing
/// requests in ipc_channel with v8 engine and put the response back to
/// ipc_channel response queue.
class Worker : public core::ServiceInterface {
 public:
  /// @brief Construct a new Worker object.
  /// @param role_id the role_id of the worker.
  /// @param function_bindings registered function bindings
  explicit Worker(
      common::RoleId role_id,
      const std::vector<std::shared_ptr<FunctionBindingObjectBase>>&
          function_bindings =
              std::vector<std::shared_ptr<FunctionBindingObjectBase>>());

  /// @brief Construct a V8 isolate for this worker.
  /// @return the execution result.
  core::ExecutionResult Init() noexcept override;

  /**
   * @brief Starts running the worker, which pops requests from ipc_channel
   * Request the queue and execute it. The function will continue to run until
   * Stop() is called. It is intended to be run in a separate process.
   *
   * @return core::ExecutionResult
   */
  core::ExecutionResult Run() noexcept override;

  /// @brief Stops the worker process and shutdown V8 isolate.
  /// @return the execution result.
  core::ExecutionResult Stop() noexcept override;

  /**
   * @brief Updates v8_isolate_ and blob_ with the code object. If
   * the code object version is newer than the current one, a new StartupData
   * blob_ will be created and v8_isolate will be update.
   * NOTE: SnapshotCreator doesn't work with wasm code. The current solution is
   * to not build blobs for wasm code requests and the code will be compiled
   * when the request is executed.
   * @param code_object the code obj that needs to be built.
   * @return core::ExecutionResult
   */
  virtual core::ExecutionResult Update(
      const ipc::RomaCodeObj& code_object) noexcept;

  virtual pid_t GetWorkerPid() { return worker_process_id_->load(); }

 protected:
  /**
   * @brief Updates v8_isolate_ and blob_ with the code object from request. If
   * the code object version is newer than the current one, a new StartupData
   * blob_ will be created and v8_isolate will be update.
   * NOTE: SnapshotCreator doesn't work with wasm code. The current solution is
   * to not build blobs for wasm code requests and the code will be compiled
   * when the request is executed.
   * @param request the request which contain code obj needs to execute.
   * @return core::ExecutionResult
   */
  virtual core::ExecutionResult Update(ipc::Request* request) noexcept;

  /**
   * @brief Executes the request with V8 engine, and puts the response back to
   * ipc_channel response queue.
   *
   * @param request the request which contain code obj needs to execute.
   * @return core::ExecutionResult
   */
  virtual core::ExecutionResult Execute(ipc::Request* request) noexcept;

 private:
  /// @brief Generate a response for the request at the top of the work
  /// container.
  /// @param request The request to generate a response for
  /// @param output The execution output to be sent with the response
  /// @param result The execution result
  core::ExecutionResult GenerateRequestResponse(
      ipc::Request*& request, const ipc::RomaString& output,
      const core::ExecutionResult& result);

  /// @brief The ipc_channel associated with this worker.
  ipc::IpcChannel& ipc_channel_;
  /// @brief Indicates the worker to stop the running process.
  std::atomic<bool>* stop_called_;

  /// @brief The execution manager leverages V8 API to persist code and
  /// execute requests.
  ExecutionManager execution_manager_;

  /// @brief User-registered C++/JS function bindings
  const std::vector<std::shared_ptr<FunctionBindingObjectBase>>&
      function_bindings_;

  /// @brief These are external references (pointers to data outside of the v8
  /// heap) which are needed for serialization of the v8 snapshot.
  std::vector<intptr_t> external_references_;

  /// @brief The PID of this worker process after being forked.
  std::atomic<pid_t>* worker_process_id_;
};
}  // namespace google::scp::roma::worker
