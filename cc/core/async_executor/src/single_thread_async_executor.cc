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

#include "single_thread_async_executor.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>

#include "async_executor_utils.h"
#include "error_codes.h"
#include "typedef.h"

using google::scp::core::common::ConcurrentQueue;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::mutex;
using std::shared_ptr;
using std::thread;
using std::unique_lock;
using std::chrono::milliseconds;

static constexpr size_t kLockWaitTimeInMilliseconds = 5;

namespace google::scp::core {
ExecutionResult SingleThreadAsyncExecutor::Init() noexcept {
  if (queue_cap_ <= 0 || queue_cap_ > kMaxQueueCap) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_INVALID_QUEUE_CAP);
  }

  normal_pri_queue_ =
      make_shared<ConcurrentQueue<shared_ptr<AsyncTask>>>(queue_cap_);
  high_pri_queue_ =
      make_shared<ConcurrentQueue<shared_ptr<AsyncTask>>>(queue_cap_);
  return SuccessExecutionResult();
};

ExecutionResult SingleThreadAsyncExecutor::Run() noexcept {
  if (is_running_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_ALREADY_RUNNING);
  }

  if (!normal_pri_queue_ || !high_pri_queue_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_INITIALIZED);
  }

  is_running_ = true;
  working_thread_ = make_unique<thread>(
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
  unique_lock<mutex> thread_lock(mutex_);

  while (true) {
    condition_variable_.wait_for(
        thread_lock, milliseconds(kLockWaitTimeInMilliseconds), [&]() {
          return !is_running_ || high_pri_queue_->Size() > 0 ||
                 normal_pri_queue_->Size() > 0;
        });

    if (normal_pri_queue_->Size() == 0 && high_pri_queue_->Size() == 0) {
      if (!is_running_) {
        break;
      }
      continue;
    }

    shared_ptr<AsyncTask> task;
    // The priority is with the high pri tasks.
    if (!high_pri_queue_->TryDequeue(task).Successful() &&
        !normal_pri_queue_->TryDequeue(task).Successful()) {
      continue;
    }

    thread_lock.unlock();
    task->Execute();
    thread_lock.lock();
  }
}

ExecutionResult SingleThreadAsyncExecutor::Stop() noexcept {
  if (!is_running_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING);
  }

  unique_lock<mutex> thread_lock(mutex_);
  is_running_ = false;

  if (drop_tasks_on_stop_) {
    shared_ptr<AsyncTask> task;
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
    std::this_thread::sleep_for(milliseconds(kSleepDurationMs));
  }

  return SuccessExecutionResult();
};

ExecutionResult SingleThreadAsyncExecutor::Schedule(
    const AsyncOperation& work, AsyncPriority priority) noexcept {
  if (!is_running_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING);
  }

  if (priority != AsyncPriority::Normal && priority != AsyncPriority::High) {
    return FailureExecutionResult(
        errors::SC_ASYNC_EXECUTOR_INVALID_PRIORITY_TYPE);
  }

  auto task = make_shared<AsyncTask>(work);
  ExecutionResult execution_result;
  if (priority == AsyncPriority::Normal) {
    execution_result = normal_pri_queue_->TryEnqueue(task);
  } else {
    execution_result = high_pri_queue_->TryEnqueue(task);
  }

  if (!execution_result.Successful()) {
    return RetryExecutionResult(errors::SC_ASYNC_EXECUTOR_EXCEEDING_QUEUE_CAP);
  }

  condition_variable_.notify_one();
  return SuccessExecutionResult();
};

ExecutionResultOr<thread::id> SingleThreadAsyncExecutor::GetThreadId() const {
  if (!is_running_.load()) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING);
  }
  return working_thread_id_;
}

}  // namespace google::scp::core
