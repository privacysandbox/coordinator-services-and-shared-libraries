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

#include "async_executor.h"

#include <chrono>
#include <functional>
#include <memory>
#include <random>
#include <vector>

#include "cc/core/interface/async_context.h"
#include "cc/core/interface/async_executor_interface.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"
#include "typedef.h"

using std::atomic;
using std::function;
using std::make_shared;
using std::memory_order_relaxed;
using std::mt19937;
using std::random_device;
using std::shared_ptr;
using std::uniform_int_distribution;
using std::vector;

namespace google::scp::core {
ExecutionResult AsyncExecutor::Init() noexcept {
  if (thread_count_ <= 0 || thread_count_ > kMaxThreadCount) {
    return FailureExecutionResult(
        errors::SC_ASYNC_EXECUTOR_INVALID_THREAD_COUNT);
  }

  if (queue_cap_ <= 0 || queue_cap_ > kMaxQueueCap) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_INVALID_QUEUE_CAP);
  }

  for (size_t i = 0; i < thread_count_; ++i) {
    urgent_task_executor_pool_.push_back(
        make_shared<SingleThreadPriorityAsyncExecutor>(queue_cap_,
                                                       drop_tasks_on_stop_));
    auto execution_result = urgent_task_executor_pool_.back()->Init();
    if (!execution_result.Successful()) {
      return execution_result;
    }
    normal_task_executor_pool_.push_back(make_shared<SingleThreadAsyncExecutor>(
        queue_cap_, drop_tasks_on_stop_));
    execution_result = normal_task_executor_pool_.back()->Init();
    if (!execution_result.Successful()) {
      return execution_result;
    }
  }

  return SuccessExecutionResult();
}

ExecutionResult AsyncExecutor::Run() noexcept {
  if (running_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_ALREADY_RUNNING);
  }

  if (urgent_task_executor_pool_.size() < thread_count_ ||
      normal_task_executor_pool_.size() < thread_count_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_INITIALIZED);
  }

  running_ = true;

  for (size_t i = 0; i < thread_count_; ++i) {
    auto execution_result = urgent_task_executor_pool_.at(i)->Run();
    if (!execution_result.Successful()) {
      return execution_result;
    }
    execution_result = normal_task_executor_pool_.at(i)->Run();
    if (!execution_result.Successful()) {
      return execution_result;
    }
  }

  return SuccessExecutionResult();
}

ExecutionResult AsyncExecutor::Stop() noexcept {
  if (!running_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING);
  }

  running_ = false;

  // Ensures all of thread are waited to finish.
  for (size_t i = 0; i < thread_count_; ++i) {
    auto execution_result = urgent_task_executor_pool_.at(i)->Stop();
    if (!execution_result.Successful()) {
      return execution_result;
    }
    execution_result = normal_task_executor_pool_.at(i)->Stop();
    if (!execution_result.Successful()) {
      return execution_result;
    }
  }

  return SuccessExecutionResult();
}

template <class TaskExecutorType>
ExecutionResult AsyncExecutor::PickTaskExecutor(
    const vector<std::shared_ptr<TaskExecutorType>>& task_executor_pool,
    TaskExecutorPoolType task_executor_pool_type,
    TaskLoadBalancingScheme task_load_balancing_scheme,
    std::shared_ptr<TaskExecutorType>& task_executor) {
  static random_device random_device_local;
  static mt19937 random_generator(random_device_local());
  static uniform_int_distribution<uint64_t> distribution;

  // Thread local task counters, initial value of the task counter with a random
  // value so that all the caller threads do not pick the same executor to start
  // with
  static thread_local atomic<uint64_t> task_counter_urgent_thread_local(
      distribution(random_generator));
  static thread_local atomic<uint64_t> task_counter_not_urgent_thread_local(
      distribution(random_generator));

  // Global task counters
  static atomic<uint64_t> task_counter_urgent(0);
  static atomic<uint64_t> task_counter_not_urgent(0);

  if (task_load_balancing_scheme ==
      TaskLoadBalancingScheme::RoundRobinPerThread) {
    if (task_executor_pool_type == TaskExecutorPoolType::UrgentPool) {
      auto picked_index =
          task_counter_urgent_thread_local.fetch_add(1, memory_order_relaxed) %
          task_executor_pool.size();
      task_executor = task_executor_pool.at(picked_index);
      return SuccessExecutionResult();
    } else if (task_executor_pool_type == TaskExecutorPoolType::NotUrgentPool) {
      auto picked_index = task_counter_not_urgent_thread_local.fetch_add(
                              1, memory_order_relaxed) %
                          task_executor_pool.size();
      task_executor = task_executor_pool.at(picked_index);
      return SuccessExecutionResult();
    } else {
      return FailureExecutionResult(
          errors::SC_ASYNC_EXECUTOR_INVALID_TASK_POOL_TYPE);
    }
  }

  if (task_load_balancing_scheme == TaskLoadBalancingScheme::Random) {
    auto picked_index =
        distribution(random_generator) % task_executor_pool.size();
    task_executor = task_executor_pool.at(picked_index);
    return SuccessExecutionResult();
  }

  if (task_load_balancing_scheme == TaskLoadBalancingScheme::RoundRobinGlobal) {
    if (task_executor_pool_type == TaskExecutorPoolType::UrgentPool) {
      auto picked_index =
          task_counter_urgent.fetch_add(1) % task_executor_pool.size();
      task_executor = task_executor_pool.at(picked_index);
      return SuccessExecutionResult();
    } else if (task_executor_pool_type == TaskExecutorPoolType::NotUrgentPool) {
      auto picked_index =
          task_counter_not_urgent.fetch_add(1) % task_executor_pool.size();
      task_executor = task_executor_pool.at(picked_index);
      return SuccessExecutionResult();
    } else {
      return FailureExecutionResult(
          errors::SC_ASYNC_EXECUTOR_INVALID_TASK_POOL_TYPE);
    }
  }

  return FailureExecutionResult(
      errors::SC_ASYNC_EXECUTOR_INVALID_LOAD_BALANCING_TYPE);
}

ExecutionResult AsyncExecutor::Schedule(const AsyncOperation& work,
                                        AsyncPriority priority) noexcept {
  if (!running_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING);
  }

  if (priority == AsyncPriority::Urgent) {
    shared_ptr<UrgentTaskExecutor> task_executor;
    auto execution_result = PickTaskExecutor(
        urgent_task_executor_pool_, TaskExecutorPoolType::UrgentPool,
        task_load_balancing_scheme_, task_executor);
    if (!execution_result.Successful()) {
      return execution_result;
    }
    AsyncTask task(work);  // Creates a task for now
    return task_executor->ScheduleFor(work, task.GetExecutionTimestamp());
  }

  if (priority == AsyncPriority::Normal || priority == AsyncPriority::High) {
    shared_ptr<NormalTaskExecutor> task_executor;
    auto execution_result = PickTaskExecutor(
        normal_task_executor_pool_, TaskExecutorPoolType::NotUrgentPool,
        task_load_balancing_scheme_, task_executor);
    if (!execution_result.Successful()) {
      return execution_result;
    }

    return task_executor->Schedule(work, priority);
  }

  return FailureExecutionResult(
      errors::SC_ASYNC_EXECUTOR_INVALID_PRIORITY_TYPE);
}

ExecutionResult AsyncExecutor::ScheduleFor(const AsyncOperation& work,
                                           Timestamp timestamp) noexcept {
  function<bool()> cancellation_callback = {};
  return ScheduleFor(work, timestamp, cancellation_callback);
}

ExecutionResult AsyncExecutor::ScheduleFor(
    const AsyncOperation& work, Timestamp timestamp,
    function<bool()>& cancellation_callback) noexcept {
  if (!running_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING);
  }

  shared_ptr<UrgentTaskExecutor> task_executor;
  auto execution_result = PickTaskExecutor(
      urgent_task_executor_pool_, TaskExecutorPoolType::UrgentPool,
      task_load_balancing_scheme_, task_executor);
  if (!execution_result.Successful()) {
    return execution_result;
  }
  return task_executor->ScheduleFor(work, timestamp, cancellation_callback);
}
}  // namespace google::scp::core
