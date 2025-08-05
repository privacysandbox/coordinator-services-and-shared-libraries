// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cc/core/async_executor/src/single_thread_async_executor.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "cc/core/async_executor/src/async_executor_utils.h"
#include "cc/core/async_executor/src/error_codes.h"
#include "cc/core/async_executor/src/typedef.h"

namespace privacy_sandbox::pbs_common {

static constexpr size_t kLockWaitTimeInMilliseconds = 5;

ExecutionResult SingleThreadAsyncExecutor::Init() noexcept {
  if (queue_cap_ <= 0 || queue_cap_ > kMaxQueueCap) {
    return FailureExecutionResult(SC_ASYNC_EXECUTOR_INVALID_QUEUE_CAP);
  }

  normal_pri_queue_ =
      std::make_shared<ConcurrentQueue<std::shared_ptr<AsyncTask>>>(queue_cap_);
  high_pri_queue_ =
      std::make_shared<ConcurrentQueue<std::shared_ptr<AsyncTask>>>(queue_cap_);
  return SuccessExecutionResult();
};

ExecutionResult SingleThreadAsyncExecutor::Run() noexcept {
  if (is_running_) {
    return FailureExecutionResult(SC_ASYNC_EXECUTOR_ALREADY_RUNNING);
  }

  if (!normal_pri_queue_ || !high_pri_queue_) {
    return FailureExecutionResult(SC_ASYNC_EXECUTOR_NOT_INITIALIZED);
  }

  is_running_ = true;
  working_thread_ = std::make_unique<std::thread>(
      [affinity_cpu_number =
           affinity_cpu_number_](SingleThreadAsyncExecutor* ptr) {
        if (affinity_cpu_number.has_value()) {
          // Ignore error.
          AsyncExecutorUtils::SetAffinity(*affinity_cpu_number);
        }
        ptr->worker_thread_started_ = true;
        ptr->StartWorker();
        ptr->worker_thread_stopped_ = true;
      },
      this);
  working_thread_id_ = working_thread_->get_id();
  working_thread_->detach();

  return SuccessExecutionResult();
}

void SingleThreadAsyncExecutor::StartWorker() noexcept {
  std::unique_lock<std::mutex> thread_lock(mutex_);

  while (true) {
    condition_variable_.wait_for(
        thread_lock, std::chrono::milliseconds(kLockWaitTimeInMilliseconds),
        [&]() {
          return !is_running_ || high_pri_queue_->Size() > 0 ||
                 normal_pri_queue_->Size() > 0;
        });

    if (normal_pri_queue_->Size() == 0 && high_pri_queue_->Size() == 0) {
      if (!is_running_) {
        break;
      }
      continue;
    }

    std::shared_ptr<AsyncTask> task;
    // The priority is with the high pri tasks.
    if (!high_pri_queue_->TryDequeue(task).Successful() &&
        !normal_pri_queue_->TryDequeue(task).Successful()) {
      continue;
    }
#if defined(PBS_ENABLE_BENCHMARKING)
    scheduling_latency_for_testing_.push_back(absl::Now() -
                                              task->GetTaskCreationTime());
#endif

    thread_lock.unlock();
    task->Execute();
    thread_lock.lock();
  }
}

ExecutionResult SingleThreadAsyncExecutor::Stop() noexcept {
  if (!is_running_) {
    return FailureExecutionResult(SC_ASYNC_EXECUTOR_NOT_RUNNING);
  }

  std::unique_lock<std::mutex> thread_lock(mutex_);
  is_running_ = false;

  if (drop_tasks_on_stop_) {
    std::shared_ptr<AsyncTask> task;
    while (normal_pri_queue_->TryDequeue(task).Successful()) {}
    while (high_pri_queue_->TryDequeue(task).Successful()) {}
  }

  condition_variable_.notify_all();
  thread_lock.unlock();

  // To ensure stop can happen cleanly, it is required to wait for the thread to
  // start and exit gracefully. If stop happens before the starting the thread,
  // there is a chance that Stop returns successful but the thread has not been
  // killed.
  while (!(worker_thread_started_.load() && worker_thread_stopped_.load())) {
    std::this_thread::sleep_for(std::chrono::milliseconds(kSleepDurationMs));
  }

  return SuccessExecutionResult();
};

ExecutionResult SingleThreadAsyncExecutor::Schedule(
    const AsyncOperation& work, AsyncPriority priority) noexcept {
  if (!is_running_) {
    return FailureExecutionResult(SC_ASYNC_EXECUTOR_NOT_RUNNING);
  }

  if (priority != AsyncPriority::Normal && priority != AsyncPriority::High) {
    return FailureExecutionResult(SC_ASYNC_EXECUTOR_INVALID_PRIORITY_TYPE);
  }

  auto task = std::make_shared<AsyncTask>(work);
  ExecutionResult execution_result;
  if (priority == AsyncPriority::Normal) {
    execution_result = normal_pri_queue_->TryEnqueue(task);
  } else {
    execution_result = high_pri_queue_->TryEnqueue(task);
  }

  if (!execution_result.Successful()) {
    return RetryExecutionResult(SC_ASYNC_EXECUTOR_EXCEEDING_QUEUE_CAP);
  }

  condition_variable_.notify_one();
  return SuccessExecutionResult();
};

ExecutionResultOr<std::thread::id> SingleThreadAsyncExecutor::GetThreadId()
    const {
#if !defined(PBS_ENABLE_BENCHMARKING)
  if (!is_running_.load()) {
    return FailureExecutionResult(SC_ASYNC_EXECUTOR_NOT_RUNNING);
  }
#endif
  return working_thread_id_;
}

}  // namespace privacy_sandbox::pbs_common
