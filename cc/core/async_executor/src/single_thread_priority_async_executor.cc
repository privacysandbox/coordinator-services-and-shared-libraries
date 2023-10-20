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

#include "single_thread_priority_async_executor.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <queue>
#include <thread>
#include <vector>

#include "core/common/time_provider/src/time_provider.h"

#include "async_executor_utils.h"
#include "error_codes.h"
#include "typedef.h"

using google::scp::core::common::TimeProvider;
using std::atomic;
using std::function;
using std::make_shared;
using std::make_unique;
using std::mutex;
using std::priority_queue;
using std::shared_ptr;
using std::thread;
using std::unique_lock;
using std::vector;
using std::chrono::milliseconds;
using std::chrono::nanoseconds;

namespace google::scp::core {
ExecutionResult SingleThreadPriorityAsyncExecutor::Init() noexcept {
  if (queue_cap_ <= 0 || queue_cap_ > kMaxQueueCap) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_INVALID_QUEUE_CAP);
  }

  queue_ = make_shared<
      priority_queue<shared_ptr<AsyncTask>, vector<shared_ptr<AsyncTask>>,
                     AsyncTaskCompareGreater>>();
  return SuccessExecutionResult();
};

ExecutionResult SingleThreadPriorityAsyncExecutor::Run() noexcept {
  if (is_running_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_ALREADY_RUNNING);
  }

  if (!queue_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_INITIALIZED);
  }

  is_running_ = true;
  working_thread_ = make_unique<thread>(
      [affinity_cpu_number =
           affinity_cpu_number_](SingleThreadPriorityAsyncExecutor* ptr) {
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

void SingleThreadPriorityAsyncExecutor::StartWorker() noexcept {
  unique_lock<mutex> thread_lock(mutex_);
  auto wait_timeout_duration_ns = kInfiniteWaitDurationNs;

  while (true) {
    condition_variable_.wait_for(thread_lock, wait_timeout_duration_ns, [&]() {
      Timestamp current_timestamp =
          TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();

      return !is_running_ || update_wait_time_ ||
             current_timestamp > next_scheduled_task_timestamp_;
    });

    if (update_wait_time_) {
      update_wait_time_ = false;
    }

    // Discard any cancelled tasks on top of the queue as an optimization to
    // avoid waiting for the future to arrive on an already cancelled task
    while (!queue_->empty() && queue_->top()->IsCancelled()) {
      queue_->pop();
    }

    if (queue_->size() == 0) {
      if (!is_running_) {
        break;
      }
      next_scheduled_task_timestamp_ = UINT64_MAX;
      wait_timeout_duration_ns = kInfiniteWaitDurationNs;
      continue;
    }

    Timestamp current_timestamp =
        TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();

    next_scheduled_task_timestamp_ = queue_->top()->GetExecutionTimestamp();
    wait_timeout_duration_ns = nanoseconds(0);
    if (current_timestamp < next_scheduled_task_timestamp_) {
      wait_timeout_duration_ns =
          nanoseconds(next_scheduled_task_timestamp_ - current_timestamp);
    } else {
      auto top = queue_->top();
      queue_->pop();
      thread_lock.unlock();
      top->Execute();
      thread_lock.lock();
    }
  }
}

ExecutionResult SingleThreadPriorityAsyncExecutor::Stop() noexcept {
  if (!is_running_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING);
  }

  unique_lock<mutex> thread_lock(mutex_);
  is_running_ = false;

  if (drop_tasks_on_stop_) {
    while (queue_->size() > 0) {
      queue_->pop();
    }
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

ExecutionResult SingleThreadPriorityAsyncExecutor::ScheduleFor(
    const AsyncOperation& work, Timestamp timestamp) noexcept {
  function<bool()> cancellation_callback = []() { return false; };
  return ScheduleFor(work, timestamp, cancellation_callback);
};

ExecutionResult SingleThreadPriorityAsyncExecutor::ScheduleFor(
    const AsyncOperation& work, Timestamp timestamp,
    function<bool()>& cancellation_callback) noexcept {
  if (!is_running_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING);
  }

  unique_lock<mutex> thread_lock(mutex_);

  if (queue_->size() >= queue_cap_) {
    return RetryExecutionResult(errors::SC_ASYNC_EXECUTOR_EXCEEDING_QUEUE_CAP);
  }

  auto task = make_shared<AsyncTask>(work, timestamp);
  cancellation_callback = [task]() mutable { return task->Cancel(); };
  queue_->push(task);

  if (timestamp < next_scheduled_task_timestamp_.load()) {
    next_scheduled_task_timestamp_ = timestamp;
    update_wait_time_ = true;
  }

  condition_variable_.notify_one();
  return SuccessExecutionResult();
};

ExecutionResultOr<thread::id> SingleThreadPriorityAsyncExecutor::GetThreadId()
    const {
  if (!is_running_.load()) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING);
  }
  return working_thread_id_;
}

}  // namespace google::scp::core
