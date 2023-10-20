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
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

#include "core/interface/async_context.h"
#include "core/interface/async_executor_interface.h"
#include "public/core/interface/execution_result.h"

#include "async_task.h"
#include "error_codes.h"
#include "single_thread_async_executor.h"
#include "single_thread_priority_async_executor.h"

static constexpr char kAsyncExecutor[] = "AsyncExecutor";

namespace google::scp::core {

/**
 * @brief Scheme to spread incoming tasks on to executor pool workers
 */
enum class TaskLoadBalancingScheme {
  /**
   * @brief Round Robin across the executors
   */
  RoundRobinGlobal = 0,
  /**
   * @brief Loosely Round Robin w.r.t thread local state
   */
  RoundRobinPerThread = 1,
  /**
   * @brief Random across the executors
   */
  Random = 2
};

/**
 * @brief Pool types for task executors.
 * NOTE: Any new thread pool added to TaskExecutor should be reflected here
 * accordingly
 */
enum class TaskExecutorPoolType { UrgentPool = 0, NotUrgentPool = 1 };

/*! @copydoc AsyncExecutorInterface
 */
class AsyncExecutor : public AsyncExecutorInterface {
 public:
  /**
   * @brief Construct a new Async Executor object with given thread_count and
   * queue_cap.
   *
   * @param thread_count the number of threads in the pool.
   * @param queue_cap the maximum size of each work queue. It is not an accurate
   * cap due to the concurrency of the queue.
   * @param drop_tasks_on_stop indicates whether the executor should wait on
   * the tasks during the stop operation.
   * @param task_load_balancing_scheme indicates the type of load balancing
   * scheme to use for the tasks
   */
  AsyncExecutor(size_t thread_count, size_t queue_cap,
                bool drop_tasks_on_stop = false,
                TaskLoadBalancingScheme task_load_balancing_scheme =
                    TaskLoadBalancingScheme::RoundRobinGlobal)
      : running_(false),
        thread_count_(thread_count),
        queue_cap_(queue_cap),
        drop_tasks_on_stop_(drop_tasks_on_stop),
        task_load_balancing_scheme_(task_load_balancing_scheme) {}

  ExecutionResult Init() noexcept override;

  ExecutionResult Run() noexcept override;

  ExecutionResult Stop() noexcept override;

  ExecutionResult Schedule(const AsyncOperation& work,
                           AsyncPriority priority) noexcept override;

  ExecutionResult Schedule(
      const AsyncOperation& work, AsyncPriority priority,
      AsyncExecutorAffinitySetting affinity) noexcept override;

  ExecutionResult ScheduleFor(const AsyncOperation& work,
                              Timestamp timestamp) noexcept override;

  ExecutionResult ScheduleFor(
      const AsyncOperation& work, Timestamp timestamp,
      AsyncExecutorAffinitySetting affinity) noexcept override;

  ExecutionResult ScheduleFor(
      const AsyncOperation& work, Timestamp timestamp,
      TaskCancellationLambda& cancellation_callback) noexcept override;

  ExecutionResult ScheduleFor(
      const AsyncOperation& work, Timestamp timestamp,
      TaskCancellationLambda& cancellation_callback,
      AsyncExecutorAffinitySetting affinity) noexcept override;

 protected:
  using UrgentTaskExecutor = SingleThreadPriorityAsyncExecutor;
  using NormalTaskExecutor = SingleThreadAsyncExecutor;

  template <class TaskExecutorType>
  ExecutionResultOr<std::shared_ptr<TaskExecutorType>> PickTaskExecutor(
      AsyncExecutorAffinitySetting affinity,
      const std::vector<std::shared_ptr<TaskExecutorType>>& task_executor_pool,
      TaskExecutorPoolType task_executor_pool_type,
      TaskLoadBalancingScheme task_load_balancing_scheme);

  /**
   * @brief While it is true, the thread pool will keep listening and
   * picking out work from work queue. While it is false, the thread pool
   * will stop listening to the work queue.
   */
  std::atomic<bool> running_;
  /// Number of threads in the thread pool.
  size_t thread_count_;
  /// The maximum length of the work queue.
  size_t queue_cap_;
  /// Indicates whether the async executor should ignore the pending tasks.
  bool drop_tasks_on_stop_;
  /// Executor pool for urgent work.
  std::vector<std::shared_ptr<UrgentTaskExecutor>> urgent_task_executor_pool_;
  /// Executor pool for normal work.
  std::vector<std::shared_ptr<NormalTaskExecutor>> normal_task_executor_pool_;
  /// A map of (normal executor and urgent executor) thread IDs and their
  /// corresponding executors (the normal executor and then the urgent executor
  /// with the same affinity).
  std::map<std::thread::id, std::pair<std::shared_ptr<NormalTaskExecutor>,
                                      std::shared_ptr<UrgentTaskExecutor>>>
      thread_id_to_executor_map_;
  /// Load balancing scheme to distribute incoming tasks on to the thread pool
  /// threads.
  TaskLoadBalancingScheme task_load_balancing_scheme_;
};
}  // namespace google::scp::core
