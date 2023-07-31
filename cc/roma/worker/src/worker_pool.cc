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

#include "worker_pool.h"

#include <chrono>
#include <thread>
#include <utility>

#include "roma/common/src/process.h"
#include "roma/ipc/src/ipc_manager.h"
#include "roma/worker/src/worker.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::roma::common::Process;
using google::scp::roma::common::RoleId;
using google::scp::roma::ipc::IpcManager;
using google::scp::roma::worker::Worker;
using std::atomic;
using std::make_unique;
using std::unique_ptr;
using std::vector;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

static constexpr int kMaxRetriesToWaitForPidUpdate = 5;

namespace google::scp::roma::worker {
WorkerPool::WorkerPool(const Config& config)
    : num_processes_(IpcManager::Instance()->GetNumProcesses()),
      worker_starter_pid_(-1) {
  for (size_t i = 0; i < num_processes_; i++) {
    workers_.push_back(nullptr);
  }
  config.GetFunctionBindings(function_bindings_);
}

ExecutionResult WorkerPool::Init() noexcept {
  {
    auto result = stop_called_shm_segment_.Create(sizeof(atomic<bool>));
    if (!result.Successful()) {
      return result;
    }
    auto stop_called_ptr = stop_called_shm_segment_.Get();
    stop_called_ = new (stop_called_ptr) atomic<bool>(false);
  }

  for (size_t idx = 0; idx < num_processes_; ++idx) {
    auto result = InitWorker(idx);
    if (!result.Successful()) {
      return result;
    }
  }
  return SuccessExecutionResult();
}

ExecutionResult WorkerPool::Run() noexcept {
  auto run_worker_starter = [this]() {
    for (size_t index = 0; index < num_processes_; index++) {
      RunWorkerProcess(index);
    }

    while (!stop_called_->load()) {
      // Wait for a child process to die
      pid_t pid_of_process_that_died = waitpid(-1, NULL, 0);
      if (stop_called_->load()) {
        break;
      }
      size_t index = GetWorkerIndex(pid_of_process_that_died);
      if (index >= 0) {
        // Restart the worker process
        RunWorkerProcess(index);
      } else {
        // We couldn't find an index for the given process
        // TODO: Log or metric
      }
    }

    return SuccessExecutionResult();
  };

  auto result = Process::Create(run_worker_starter, worker_starter_pid_);
  if (!result.Successful()) {
    return result;
  }

  return SuccessExecutionResult();
}

ExecutionResult WorkerPool::Stop() noexcept {
  stop_called_->store(true);

  for (size_t idx = 0; idx < num_processes_; ++idx) {
    auto result = StopWorker(idx);
    if (!result.Successful()) {
      return result;
    }
  }
  return SuccessExecutionResult();
}

vector<pid_t> WorkerPool::GetWorkerPids() {
  vector<pid_t> worker_pids;
  for (size_t i = 0; i < num_processes_; i++) {
    auto pid = workers_.at(i)->GetWorkerPid();
    worker_pids.push_back(pid);
  }
  return worker_pids;
}

pid_t WorkerPool::GetWorkerStarterPid() {
  return worker_starter_pid_;
}

ExecutionResult WorkerPool::InitWorker(size_t index) {
  auto role_id = RoleId(index, false);
  auto worker = make_unique<Worker>(role_id, function_bindings_);
  workers_.at(index).swap(worker);
  auto result = workers_.at(index)->Init();
  if (!result) {
    return result;
  }

  return SuccessExecutionResult();
}

ExecutionResult WorkerPool::RunWorker(size_t index) {
  auto role_id = RoleId(index, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
  return workers_.at(index)->Run();
}

ExecutionResult WorkerPool::StopWorker(size_t index) {
  auto result = workers_.at(index)->Stop();
  if (!result.Successful()) {
    return result;
  }

  return SuccessExecutionResult();
}

core::ExecutionResult WorkerPool::RunWorkerProcess(size_t index) {
  auto run_worker = [&, index]() { return RunWorker(index); };

  pid_t worker_pid;
  auto result = Process::Create(run_worker, worker_pid);
  if (!result.Successful()) {
    return result;
  }

  // Stall here until we verify that the worker process has started.
  // However, don't hang forever.
  int retries = 0;
  while (GetWorkerPids().at(index) != worker_pid &&
         retries < kMaxRetriesToWaitForPidUpdate) {
    sleep_for(milliseconds(20));
    retries++;
  }

  return SuccessExecutionResult();
}

size_t WorkerPool::GetWorkerIndex(pid_t pid) {
  for (size_t index = 0; index < num_processes_; index++) {
    if (GetWorkerPids().at(index) == pid) {
      return index;
    }
  }
  return -1;
}
}  // namespace google::scp::roma::worker
