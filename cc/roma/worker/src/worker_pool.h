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

#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"
#include "roma/ipc/src/ipc_manager.h"
#include "roma/worker/src/worker.h"

#include "error_codes.h"

namespace google::scp::roma::worker {

class WorkerPool : public core::ServiceInterface {
 public:
  /// Constructs a worker pool with \a num_processes workers.
  explicit WorkerPool(const Config& config = Config());

  virtual ~WorkerPool() {}

  virtual core::ExecutionResult Init() noexcept;
  virtual core::ExecutionResult Run() noexcept;
  virtual core::ExecutionResult Stop() noexcept;

  /**
   * @brief Get the Worker PIDs.
   * This gets the PIDs of the worker processes.
   * NOTE: Some of the entries might be -1 because the processes are still being
   * created. Also, if worker processes are restarted, the PIDs might change, so
   * these PIDs should always be tested before assuming they are valid PIDs.
   * These PIDs won't be of direct children of the calling process as
   * they are forked from an intermediate, hence they CANNOT be waitpid'd from
   * the calling process.
   *
   * @return std::vector<pid_t>
   */
  virtual std::vector<pid_t> GetWorkerPids();

  /**
   * @brief Get the PID of the internal process which starts all the worker.
   * This process is a direct child of the calling process, hence it CAN be
   * waitpid'd
   *
   * @return pid_t
   */
  virtual pid_t GetWorkerStarterPid();

 protected:
  /**
   * @brief Initialize a worker
   *
   * @param index The worker index
   * @return core::ExecutionResult
   */
  core::ExecutionResult InitWorker(size_t index);

  /**
   * @brief Run a worker
   *
   * @param index The worker index
   * @return core::ExecutionResult
   */
  core::ExecutionResult RunWorker(size_t index);

  /**
   * @brief Stop a worker
   *
   * @param index The worker index
   * @return core::ExecutionResult
   */
  core::ExecutionResult StopWorker(size_t index);

  /**
   * @brief Run a worker process
   *
   * @param index The worker index
   * @return core::ExecutionResult
   */
  core::ExecutionResult RunWorkerProcess(size_t index);

  /**
   * @brief Get a worker's index based its expected PID
   *
   * @param pid The expected PID
   * @return The index
   */
  size_t GetWorkerIndex(pid_t pid);

  /// @brief  Number of processes which equals to the number of workers.
  const size_t num_processes_;
  /// @brief A vector stores all worker instances.
  std::vector<std::unique_ptr<Worker>> workers_;
  /// @brief User-registered C++/JS function bindings.
  std::vector<std::shared_ptr<FunctionBindingObjectBase>> function_bindings_;

  /// @brief memory segment to allocate the stop called flag.
  ipc::SharedMemorySegment stop_called_shm_segment_;
  /// @brief whether the service has been stopped.
  std::atomic<bool>* stop_called_;

  /// @brief  The PID of the internal worker starter process.
  pid_t worker_starter_pid_;
};

}  // namespace google::scp::roma::worker
