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

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>

#include "core/common/concurrent_queue/src/concurrent_queue.h"
#include "core/interface/async_executor_interface.h"

#include "async_task.h"

namespace google::scp::core {
/**
 * @brief A single threaded async executor. This executor will have one thread
 * working with one queue.
 */
class SingleThreadAsyncExecutor : ServiceInterface {
 public:
  explicit SingleThreadAsyncExecutor(size_t queue_cap,
                                     bool drop_tasks_on_stop = false)
      : is_running_(false),
        worker_thread_started_(false),
        worker_thread_stopped_(false),
        queue_cap_(queue_cap),
        drop_tasks_on_stop_(drop_tasks_on_stop) {}

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  /**
   * @brief Schedules a task with certain priority to be execute immediately or
   * deferred.
   * @param work the task that needs to be scheduled.
   * @param priority the priority of the task. Either normal or medium.
   * @return ExecutionResult result of the execution with possible error code.
   */
  ExecutionResult Schedule(const AsyncOperation& work,
                           AsyncPriority priority) noexcept;

 private:
  /// Starts the internal worker thread.
  void StartWorker() noexcept;

  /**
   * @brief While it is true, the running thread will keep listening and
   * picking out work from work queue. While it is false, the thread will try to
   * finish all the remaining tasks in the queue and then stop.
   */
  std::atomic<bool> is_running_;
  /// Indicates whether the worker thread started.
  std::atomic<bool> worker_thread_started_;
  /// Indicates whether the worker thread stopped.
  std::atomic<bool> worker_thread_stopped_;
  /// The maximum length of the work queue.
  size_t queue_cap_;
  /// Indicates whether the async executor should ignore the pending tasks.
  bool drop_tasks_on_stop_;
  /// Queue for accepting the incoming normal priority tasks.
  std::shared_ptr<common::ConcurrentQueue<std::shared_ptr<AsyncTask>>>
      normal_pri_queue_;
  /// Queue for accepting the incoming high priority tasks.
  std::shared_ptr<common::ConcurrentQueue<std::shared_ptr<AsyncTask>>>
      high_pri_queue_;
  /// A unique pointer to the working thread.
  std::unique_ptr<std::thread> working_thread_;
  /**
   * @brief Used in combination with the condition variable for signaling the
   * thread that an element is pushed to the queue.
   */
  std::mutex mutex_;
  /**
   * @brief Used in combination with the mutex for signaling the thread that an
   * element is pushed to the queue.
   */
  std::condition_variable condition_variable_;
};
}  // namespace google::scp::core
