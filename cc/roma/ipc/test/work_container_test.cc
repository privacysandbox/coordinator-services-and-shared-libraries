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

#include "roma/ipc/src/work_container.h"

#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <gmock/gmock.h>

#include "core/test/utils/conditional_wait.h"
#include "roma/common/src/process.h"
#include "roma/common/src/shared_memory.h"
#include "roma/common/src/shared_memory_pool.h"

using std::make_unique;
using std::move;
using std::set;
using std::string;
using std::thread;
using std::to_string;
using std::unique_ptr;
using std::vector;
using ::testing::HasSubstr;

namespace google::scp::roma::ipc::test {
using common::Process;
using common::RomaString;
using common::SharedMemoryPool;
using common::SharedMemorySegment;
using common::ShmAllocated;
using core::SuccessExecutionResult;
using core::test::WaitUntil;
using ipc::Request;
using ipc::Response;
using ipc::ResponseStatus;
using ipc::RomaCodeObj;
using ipc::WorkContainer;
using ipc::WorkItem;

/**
 * @brief The use case is that the dispatcher process puts work items in the
 * container, and the dispatcher process also polls the container for completed
 * items in a separate thread. Conversely, the worker process will pick up items
 * from the container and mark them as completed once done.
 */
TEST(WorkContainerTest, BasicE2E) {
  SharedMemorySegment segment;
  segment.Create(5 * 10240);
  auto* pool = new (segment.Get()) SharedMemoryPool();
  pool->Init(reinterpret_cast<uint8_t*>(segment.Get()) + sizeof(*pool),
             segment.Size() - sizeof(*pool));
  pool->SetThisThreadMemPool();

  auto* container = new WorkContainer(*pool);
  constexpr int total_items = 10;

  auto worker_process = [&container]() {
    set<string> request_ids;

    for (int i = 0; i < total_items; i++) {
      Request* request;
      auto result = container->GetRequest(request);
      EXPECT_EQ(result, SuccessExecutionResult());
      EXPECT_THAT(request->code_obj->id, HasSubstr("REQ_ID"));
      request_ids.insert(string(request->code_obj->id.c_str()));

      auto response = make_unique<Response>();
      response->status = ResponseStatus::kSucceeded;

      result = container->CompleteRequest(move(response));
      EXPECT_EQ(result, SuccessExecutionResult());
    }

    for (int i = 0; i < total_items; i++) {
      EXPECT_TRUE(request_ids.find("REQ_ID" + to_string(i)) !=
                  request_ids.end());
    }

    return SuccessExecutionResult();
  };

  pid_t worker_process_pid;
  auto result = Process::Create(worker_process, worker_process_pid);
  EXPECT_GT(worker_process_pid, 0);
  EXPECT_EQ(result, SuccessExecutionResult());

  for (int i = 0; i < total_items; i++) {
    auto work_item = make_unique<WorkItem>();
    work_item->request = make_unique<Request>();
    CodeObject code_obj = {.id = "REQ_ID" + to_string(i)};
    work_item->request->code_obj = make_unique<RomaCodeObj>(code_obj);
    EXPECT_TRUE(container->TryAcquireAdd().Successful());
    auto result = container->Add(move(work_item));
    EXPECT_EQ(result, SuccessExecutionResult());
  }

  std::atomic<bool> completed_work_thread_done(false);
  auto get_completed_work_thread =
      thread([&container, &completed_work_thread_done, &pool]() {
        pool->SetThisThreadMemPool();

        for (int i = 0; i < total_items; i++) {
          unique_ptr<WorkItem> completed;
          auto result = container->GetCompleted(completed);
          EXPECT_EQ(result, SuccessExecutionResult());
          EXPECT_TRUE(completed->Succeeded());
        }

        completed_work_thread_done.store(true);
      });

  int status;
  waitpid(worker_process_pid, &status, 0);
  // If WIFEXITED returns zero, then the process died abnormally
  ASSERT_NE(WIFEXITED(status), 0);
  EXPECT_EQ(WEXITSTATUS(status), 0);

  WaitUntil([&]() { return completed_work_thread_done.load(); });

  EXPECT_EQ(container->Size(), 0);

  if (get_completed_work_thread.joinable()) {
    get_completed_work_thread.join();
  }

  delete container;
}

/**
 * @brief The work container uses a circular buffer, so we want to make sure
 * that the circular nature of the container is working as intended. And also
 * validate that the Add method can be called from multiple threads.
 */
TEST(WorkContainerTest, WrapAroundSeveralTimes) {
  SharedMemorySegment segment;
  segment.Create(5 * 10240);
  auto* pool = new (segment.Get()) SharedMemoryPool();
  pool->Init(reinterpret_cast<uint8_t*>(segment.Get()) + sizeof(*pool),
             segment.Size() - sizeof(*pool));
  pool->SetThisThreadMemPool();

  auto* container = new WorkContainer(*pool, /* capacity */ 10);

  vector<thread> threads;
  constexpr int num_threads = 101;
  threads.reserve(num_threads);

  // We could potentially have multiple threads pushing work
  for (int i = 0; i < num_threads; i++) {
    // Add work threads
    threads.push_back(thread([i, &container, &pool]() {
      pool->SetThisThreadMemPool();

      auto work_item = make_unique<WorkItem>();
      work_item->request = make_unique<Request>();
      CodeObject code_obj = {.id = "REQ_ID" + to_string(i)};
      work_item->request->code_obj = make_unique<RomaCodeObj>(code_obj);

      // We need to spin here since we're waiting for spots on the container
      while (!container->TryAcquireAdd().Successful()) {}

      container->Add(move(work_item));
    }));
  }

  // In our use case, we have only one work thread
  auto work_process_thread = thread([&container, &pool]() {
    pool->SetThisThreadMemPool();

    for (int i = 0; i < num_threads; i++) {
      Request* request;
      auto result = container->GetRequest(request);
      EXPECT_EQ(result, SuccessExecutionResult());

      auto response = make_unique<Response>();
      response->status = ResponseStatus::kSucceeded;

      result = container->CompleteRequest(move(response));
      EXPECT_EQ(result, SuccessExecutionResult());
    }
  });

  // In our use case, we have only one thread getting completed work
  auto get_completed_work_thread = thread([&container, &pool]() {
    pool->SetThisThreadMemPool();

    set<string> request_ids;

    for (int i = 0; i < num_threads; i++) {
      unique_ptr<WorkItem> completed;
      auto result = container->GetCompleted(completed);
      request_ids.insert(string(completed->request->code_obj->id.c_str()));
      EXPECT_EQ(result, SuccessExecutionResult());
      EXPECT_TRUE(completed->Succeeded());
    }

    for (int i = 0; i < num_threads; i++) {
      EXPECT_TRUE(request_ids.find("REQ_ID" + to_string(i)) !=
                  request_ids.end());
    }
  });

  if (work_process_thread.joinable()) {
    work_process_thread.join();
  }

  if (get_completed_work_thread.joinable()) {
    get_completed_work_thread.join();
  }

  for (auto& t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  EXPECT_EQ(container->Size(), 0);
  delete container;
}

TEST(WorkContainerTest, QueueFunctionality) {
  SharedMemorySegment segment;
  segment.Create(10240);
  auto* pool = new (segment.Get()) SharedMemoryPool();
  pool->Init(reinterpret_cast<uint8_t*>(segment.Get()) + sizeof(*pool),
             segment.Size() - sizeof(*pool));
  pool->SetThisThreadMemPool();

  auto* container = new WorkContainer(*pool, /* capacity */ 10);

  // Insert requests
  for (int i = 0; i < 10; i++) {
    auto work_item = make_unique<WorkItem>();
    work_item->request = make_unique<Request>();
    CodeObject code_obj = {.id = "REQ_ID" + to_string(i)};
    work_item->request->code_obj = make_unique<RomaCodeObj>(code_obj);
    EXPECT_TRUE(container->TryAcquireAdd().Successful());
    container->Add(move(work_item));
  }

  // Get and process requests
  for (int i = 0; i < 10; i++) {
    Request* request;
    auto result = container->GetRequest(request);
    EXPECT_EQ(result, SuccessExecutionResult());
    auto request_id = string(request->code_obj->id.c_str());
    // Should be in the order they were inserted
    EXPECT_EQ("REQ_ID" + to_string(i), request_id);

    auto response = make_unique<Response>();
    response->status = ResponseStatus::kSucceeded;

    result = container->CompleteRequest(move(response));
    EXPECT_EQ(result, SuccessExecutionResult());
  }

  // Get completed requests
  for (int i = 0; i < 10; i++) {
    unique_ptr<WorkItem> completed;
    auto result = container->GetCompleted(completed);
    auto request_id = string(completed->request->code_obj->id.c_str());
    // Should be in the order they were inserted
    EXPECT_EQ("REQ_ID" + to_string(i), request_id);
    EXPECT_EQ(result, SuccessExecutionResult());
    EXPECT_TRUE(completed->Succeeded());
  }

  EXPECT_EQ(container->Size(), 0);
  delete container;
}

TEST(WorkContainerTest, TryAcquireAddShouldFailWhenTheContainerIsFull) {
  SharedMemorySegment segment;
  segment.Create(10240);
  auto* pool = new (segment.Get()) SharedMemoryPool();
  pool->Init(reinterpret_cast<uint8_t*>(segment.Get()) + sizeof(*pool),
             segment.Size() - sizeof(*pool));
  pool->SetThisThreadMemPool();

  auto* container = new WorkContainer(*pool, /* capacity */ 10);

  // Insert requests
  for (int i = 0; i < 10; i++) {
    auto work_item = make_unique<WorkItem>();
    work_item->request = make_unique<Request>();
    CodeObject code_obj = {.id = "REQ_ID" + to_string(i)};
    work_item->request->code_obj = make_unique<RomaCodeObj>(code_obj);
    EXPECT_TRUE(container->TryAcquireAdd().Successful());
    container->Add(move(work_item));
  }

  // Container is full
  EXPECT_EQ(container->Size(), 10);

  EXPECT_FALSE(container->TryAcquireAdd().Successful());

  delete container;
}
}  // namespace google::scp::roma::ipc::test
