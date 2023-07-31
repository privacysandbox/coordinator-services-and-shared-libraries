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

#include "work_container.h"

#include <utility>

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_ROMA_WORK_CONTAINER_STOPPED;
using std::lock_guard;
using std::move;
using std::unique_ptr;

namespace google::scp::roma::ipc {

ExecutionResult WorkContainer::TryAcquireAdd() {
  auto ctx = SharedMemoryPool::SwitchTo(mem_pool_);
  return space_available_semaphore_.TryWait();
}

ExecutionResult WorkContainer::Add(unique_ptr<WorkItem> work_item) {
  auto ctx = SharedMemoryPool::SwitchTo(mem_pool_);

  {
    lock_guard lock(add_item_mutex_);
    items_[add_index_++] = move(work_item);
    add_index_ = add_index_ % capacity_;
  }

  size_++;

  acquire_semaphore_.Signal();

  return SuccessExecutionResult();
}

ExecutionResult WorkContainer::GetRequest(Request*& request) {
  auto ctx = SharedMemoryPool::SwitchTo(mem_pool_);

  // We need to check the stop flag twice. This is so that if the semaphore is
  // being held and it was released for the sole purpose of stopping, then this
  // is picked up. And also so that subsequent calls to this function after
  // it's been stopped (if any), don't block.
  if (stop_.load()) {
    return FailureExecutionResult(SC_ROMA_WORK_CONTAINER_STOPPED);
  }
  acquire_semaphore_.WaitOne();
  if (stop_.load()) {
    return FailureExecutionResult(SC_ROMA_WORK_CONTAINER_STOPPED);
  }

  request = items_[acquire_index_]->request.get();

  return SuccessExecutionResult();
}

ExecutionResult WorkContainer::CompleteRequest(
    std::unique_ptr<Response> response) {
  auto ctx = SharedMemoryPool::SwitchTo(mem_pool_);

  items_[acquire_index_++]->Complete(move(response));
  acquire_index_ = acquire_index_ % capacity_;

  complete_semaphore_.Signal();

  return SuccessExecutionResult();
}

ExecutionResult WorkContainer::GetCompleted(unique_ptr<WorkItem>& work_item) {
  auto ctx = SharedMemoryPool::SwitchTo(mem_pool_);

  // We need to check the stop flag twice. This is so that if the semaphore is
  // being held and it was released for the sole purpose of stopping, then this
  // is picked up. And also so that subsequent calls to this function after
  // it's been stopped (if any), don't block.
  if (stop_.load()) {
    return FailureExecutionResult(SC_ROMA_WORK_CONTAINER_STOPPED);
  }
  complete_semaphore_.WaitOne();
  if (stop_.load()) {
    return FailureExecutionResult(SC_ROMA_WORK_CONTAINER_STOPPED);
  }

  work_item.swap(items_[get_complete_index_++]);
  get_complete_index_ = get_complete_index_ % capacity_;

  size_--;

  space_available_semaphore_.Signal();

  return SuccessExecutionResult();
}

size_t WorkContainer::Size() {
  return size_.load();
}

void WorkContainer::ReleaseLocks() {
  stop_.store(true);
  ReleaseGetRequestLock();
  complete_semaphore_.Signal();
}

void WorkContainer::ReleaseGetRequestLock() {
  acquire_semaphore_.Signal();
}
}  // namespace google::scp::roma::ipc
