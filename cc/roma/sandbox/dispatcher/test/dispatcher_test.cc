/*
 * Copyright 2023 Google LLC
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

#include "roma/sandbox/dispatcher/src/dispatcher.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "core/async_executor/src/async_executor.h"
#include "core/test/utils/auto_init_run_stop.h"
#include "core/test/utils/conditional_wait.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "roma/interface/roma.h"
#include "roma/sandbox/worker_api/src/worker_api.h"
#include "roma/sandbox/worker_api/src/worker_api_sapi.h"
#include "roma/sandbox/worker_pool/src/worker_pool.h"
#include "roma/sandbox/worker_pool/src/worker_pool_api_sapi.h"

using absl::flat_hash_set;
using absl::StatusOr;
using google::scp::core::AsyncExecutor;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::test::AutoInitRunStop;
using google::scp::core::test::WaitUntil;
using google::scp::roma::sandbox::worker_api::WorkerApi;
using google::scp::roma::sandbox::worker_api::WorkerApiSapi;
using google::scp::roma::sandbox::worker_api::WorkerApiSapiConfig;
using google::scp::roma::sandbox::worker_pool::WorkerPool;
using google::scp::roma::sandbox::worker_pool::WorkerPoolApiSapi;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;

namespace google::scp::roma::sandbox::dispatcher::test {
TEST(DispatcherTest, CanRunCode) {
  auto async_executor = make_shared<AsyncExecutor>(1, 10);

  vector<WorkerApiSapiConfig> configs;
  WorkerApiSapiConfig config;
  config.worker_js_engine = worker::WorkerFactory::WorkerEngine::v8;
  config.js_engine_require_code_preload = true;
  config.compilation_context_cache_size = 5;
  config.native_js_function_comms_fd = -1;
  config.native_js_function_names = vector<string>();
  configs.push_back(config);

  shared_ptr<WorkerPool> worker_pool =
      make_shared<WorkerPoolApiSapi>(configs, 1);
  AutoInitRunStop for_async_executor(*async_executor);
  AutoInitRunStop for_worker_pool(*worker_pool);

  Dispatcher dispatcher(async_executor, worker_pool, 10, 5);
  AutoInitRunStop for_dispatcher(dispatcher);

  auto load_request = make_unique<CodeObject>();
  load_request->id = "some_id";
  load_request->version_num = 1;
  load_request->js =
      "function test(input) { return input + \" Some string\"; }";

  atomic<bool> done_loading(false);

  auto result = dispatcher.Dispatch(
      move(load_request),
      [&done_loading](unique_ptr<StatusOr<ResponseObject>> resp) {
        EXPECT_TRUE(resp->ok());
        done_loading.store(true);
      });
  EXPECT_SUCCESS(result);

  WaitUntil([&done_loading]() { return done_loading.load(); });

  auto execute_request = make_unique<InvocationRequestStrInput>();
  execute_request->id = "some_id";
  execute_request->version_num = 1;
  execute_request->handler_name = "test";
  execute_request->input.push_back("\"Hello\"");

  atomic<bool> done_executing(false);

  result = dispatcher.Dispatch(
      move(execute_request),
      [&done_executing](unique_ptr<StatusOr<ResponseObject>> resp) {
        EXPECT_TRUE(resp->ok());
        EXPECT_EQ("\"Hello Some string\"", (*resp)->resp);
        done_executing.store(true);
      });

  EXPECT_SUCCESS(result);

  WaitUntil([&done_executing]() { return done_executing.load(); });
}

TEST(DispatcherTest, CanHandleCodeFailures) {
  auto async_executor = make_shared<AsyncExecutor>(1, 10);

  vector<WorkerApiSapiConfig> configs;
  WorkerApiSapiConfig config;
  config.worker_js_engine = worker::WorkerFactory::WorkerEngine::v8;
  config.js_engine_require_code_preload = true;
  config.compilation_context_cache_size = 5;
  config.native_js_function_comms_fd = -1;
  config.native_js_function_names = vector<string>();
  configs.push_back(config);

  shared_ptr<WorkerPool> worker_pool =
      make_shared<WorkerPoolApiSapi>(configs, 1);
  AutoInitRunStop for_async_executor(*async_executor);
  AutoInitRunStop for_worker_pool(*worker_pool);

  Dispatcher dispatcher(async_executor, worker_pool, 10, 5);
  AutoInitRunStop for_dispatcher(dispatcher);

  auto load_request = make_unique<CodeObject>();
  load_request->id = "some_id";
  load_request->version_num = 1;
  // Bad JS
  load_request->js = "function test(input) { ";

  atomic<bool> done_loading(false);

  auto result = dispatcher.Dispatch(
      move(load_request),
      [&done_loading](unique_ptr<StatusOr<ResponseObject>> resp) {
        // That didn't work
        EXPECT_FALSE(resp->ok());
        done_loading.store(true);
      });
  EXPECT_SUCCESS(result);

  WaitUntil([&done_loading]() { return done_loading.load(); });
}

TEST(DispatcherTest, CanHandleExecuteWithoutLoadFailure) {
  auto async_executor = make_shared<AsyncExecutor>(1, 10);

  vector<WorkerApiSapiConfig> configs;
  WorkerApiSapiConfig config;
  config.worker_js_engine = worker::WorkerFactory::WorkerEngine::v8;
  config.js_engine_require_code_preload = true;
  config.compilation_context_cache_size = 5;
  config.native_js_function_comms_fd = -1;
  config.native_js_function_names = vector<string>();
  configs.push_back(config);

  shared_ptr<WorkerPool> worker_pool =
      make_shared<WorkerPoolApiSapi>(configs, 1);
  AutoInitRunStop for_async_executor(*async_executor);
  AutoInitRunStop for_worker_pool(*worker_pool);

  Dispatcher dispatcher(async_executor, worker_pool, 10, 5);
  AutoInitRunStop for_dispatcher(dispatcher);

  auto execute_request = make_unique<InvocationRequestStrInput>();
  execute_request->id = "some_id";
  execute_request->version_num = 1;
  execute_request->handler_name = "test";
  execute_request->input.push_back("\"Hello\"");

  atomic<bool> done_executing(false);

  auto result = dispatcher.Dispatch(
      move(execute_request),
      [&done_executing](unique_ptr<StatusOr<ResponseObject>> resp) {
        EXPECT_FALSE(resp->ok());
        done_executing.store(true);
      });

  EXPECT_SUCCESS(result);

  WaitUntil([&done_executing]() { return done_executing.load(); });
}

TEST(DispatcherTest, BroadcastShouldUpdateAllWorkers) {
  const size_t number_of_workers = 5;
  auto async_executor = make_shared<AsyncExecutor>(number_of_workers, 100);

  vector<WorkerApiSapiConfig> configs;
  for (int i = 0; i < number_of_workers; i++) {
    WorkerApiSapiConfig config;
    config.worker_js_engine = worker::WorkerFactory::WorkerEngine::v8;
    config.js_engine_require_code_preload = true;
    config.compilation_context_cache_size = 5;
    config.native_js_function_comms_fd = -1;
    config.native_js_function_names = vector<string>();
    configs.push_back(config);
  }

  shared_ptr<WorkerPool> worker_pool =
      make_shared<WorkerPoolApiSapi>(configs, number_of_workers);
  AutoInitRunStop for_async_executor(*async_executor);
  AutoInitRunStop for_worker_pool(*worker_pool);

  Dispatcher dispatcher(async_executor, worker_pool, 100, 5);
  AutoInitRunStop for_dispatcher(dispatcher);

  auto load_request = make_unique<CodeObject>();
  load_request->id = "some_id";
  load_request->version_num = 1;
  load_request->js =
      "function test(input) { return input + \" Some string\"; }";

  atomic<bool> done_loading(false);

  auto result = dispatcher.Broadcast(
      move(load_request),
      [&done_loading](unique_ptr<StatusOr<ResponseObject>> resp) {
        EXPECT_TRUE(resp->ok());
        done_loading.store(true);
      });
  EXPECT_SUCCESS(result);

  WaitUntil([&done_loading]() { return done_loading.load(); });

  atomic<int> execution_count(0);
  // More than the number of workers to make sure the requests can indeed run in
  // all workers.
  int requests_sent = number_of_workers * 3;

  for (int i = 0; i < requests_sent; i++) {
    auto execute_request = make_unique<InvocationRequestStrInput>();
    execute_request->id = "some_id" + to_string(i);
    execute_request->version_num = 1;
    execute_request->handler_name = "test";
    execute_request->input.push_back("\"Hello" + to_string(i) + "\"");

    result = dispatcher.Dispatch(
        move(execute_request),
        [&execution_count, i](unique_ptr<StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          EXPECT_EQ("\"Hello" + to_string(i) + " Some string\"", (*resp)->resp);
          execution_count++;
        });

    EXPECT_SUCCESS(result);
  }

  WaitUntil([&execution_count, requests_sent]() {
    return execution_count.load() >= requests_sent;
  });
}

TEST(DispatcherTest, BroadcastShouldExitGracefullyIfThereAreErrorsWithTheCode) {
  const size_t number_of_workers = 5;
  auto async_executor = make_shared<AsyncExecutor>(number_of_workers, 100);

  vector<WorkerApiSapiConfig> configs;
  for (int i = 0; i < number_of_workers; i++) {
    WorkerApiSapiConfig config;
    config.worker_js_engine = worker::WorkerFactory::WorkerEngine::v8;
    config.js_engine_require_code_preload = true;
    config.compilation_context_cache_size = 5;
    config.native_js_function_comms_fd = -1;
    config.native_js_function_names = vector<string>();
    configs.push_back(config);
  }

  shared_ptr<WorkerPool> worker_pool =
      make_shared<WorkerPoolApiSapi>(configs, number_of_workers);
  AutoInitRunStop for_async_executor(*async_executor);
  AutoInitRunStop for_worker_pool(*worker_pool);

  Dispatcher dispatcher(async_executor, worker_pool, 100, 5);
  AutoInitRunStop for_dispatcher(dispatcher);

  auto load_request = make_unique<CodeObject>();
  load_request->id = "some_id";
  load_request->version_num = 1;
  // Bad syntax
  load_request->js = "function test(input) { return";

  atomic<bool> done_loading(false);

  auto result = dispatcher.Broadcast(
      move(load_request),
      [&done_loading](unique_ptr<StatusOr<ResponseObject>> resp) {
        // That failed
        EXPECT_FALSE(resp->ok());
        done_loading.store(true);
      });
  EXPECT_SUCCESS(result);

  WaitUntil([&done_loading]() { return done_loading.load(); });
}

TEST(DispatcherTest, DispatchBatchShouldExecuteAllRequests) {
  const size_t number_of_workers = 5;
  auto async_executor = make_shared<AsyncExecutor>(number_of_workers, 100);

  vector<WorkerApiSapiConfig> configs;
  for (int i = 0; i < number_of_workers; i++) {
    WorkerApiSapiConfig config;
    config.worker_js_engine = worker::WorkerFactory::WorkerEngine::v8;
    config.js_engine_require_code_preload = true;
    config.compilation_context_cache_size = 5;
    config.native_js_function_comms_fd = -1;
    config.native_js_function_names = vector<string>();
    configs.push_back(config);
  }

  shared_ptr<WorkerPool> worker_pool =
      make_shared<WorkerPoolApiSapi>(configs, number_of_workers);
  AutoInitRunStop for_async_executor(*async_executor);
  AutoInitRunStop for_worker_pool(*worker_pool);

  Dispatcher dispatcher(async_executor, worker_pool, 100, 5);
  AutoInitRunStop for_dispatcher(dispatcher);

  auto load_request = make_unique<CodeObject>();
  load_request->id = "some_id";
  load_request->version_num = 1;
  load_request->js =
      "function test(input) { return input + \" Some string\"; }";

  atomic<bool> done_loading(false);

  auto result = dispatcher.Broadcast(
      move(load_request),
      [&done_loading](unique_ptr<StatusOr<ResponseObject>> resp) {
        EXPECT_TRUE(resp->ok());
        done_loading.store(true);
      });
  EXPECT_SUCCESS(result);

  WaitUntil([&done_loading]() { return done_loading.load(); });

  // More than the number of workers to make sure the requests can indeed run in
  // all workers.
  int requests_sent = number_of_workers * 3;

  vector<InvocationRequestStrInput> batch;
  flat_hash_set<string> request_ids;

  for (int i = 0; i < requests_sent; i++) {
    auto execute_request = InvocationRequestStrInput();
    execute_request.id = "some_id" + to_string(i);
    execute_request.version_num = 1;
    execute_request.handler_name = "test";
    execute_request.input.push_back("\"Hello" + to_string(i) + "\"");

    // Keep track of the request ids
    request_ids.insert(execute_request.id);
    batch.push_back(execute_request);
  }

  atomic<bool> finished_batch(false);
  vector<StatusOr<ResponseObject>> test_batch_response;

  dispatcher.DispatchBatch(
      batch, [&finished_batch, &test_batch_response](
                 const vector<StatusOr<ResponseObject>>& batch_response) {
        for (auto& r : batch_response) {
          test_batch_response.push_back(r);
        }
        finished_batch.store(true);
      });

  WaitUntil([&finished_batch]() { return finished_batch.load(); });

  for (auto& r : test_batch_response) {
    EXPECT_TRUE(r.ok());
    // Remove the ids we see form the set
    request_ids.erase(r->id);
  }

  // Since we should have a gotten a response for all request ID, we expect all
  // the ids to have been removed from this set.
  EXPECT_TRUE(request_ids.empty());
}

TEST(DispatcherTest, DispatchBatchShouldFailIfQueuesAreFull) {
  // One worker with a one-item queue so that the queue takes long to empty out
  const size_t number_of_workers = 1;
  auto async_executor = make_shared<AsyncExecutor>(
      number_of_workers /*thread_count*/, 1 /*queue_cap*/);

  WorkerApiSapiConfig config;
  config.worker_js_engine = worker::WorkerFactory::WorkerEngine::v8;
  config.js_engine_require_code_preload = true;
  config.compilation_context_cache_size = 5;
  config.native_js_function_comms_fd = -1;
  config.native_js_function_names = vector<string>();

  vector<WorkerApiSapiConfig> configs = {config};
  shared_ptr<WorkerPool> worker_pool =
      make_shared<WorkerPoolApiSapi>(configs, number_of_workers);
  AutoInitRunStop for_async_executor(*async_executor);
  AutoInitRunStop for_worker_pool(*worker_pool);

  Dispatcher dispatcher(async_executor, worker_pool,
                        100 /*max_pending_requests*/, 5 /*code_version_size*/);
  AutoInitRunStop for_dispatcher(dispatcher);

  auto load_request = make_unique<CodeObject>();
  load_request->id = "some_id";
  load_request->version_num = 1;
  // Function that takes long so that queues will have items in it
  load_request->js = R"""(
    function sleep(milliseconds) {
      const date = Date.now();
      let currentDate = null;
      do {
        currentDate = Date.now();
      } while (currentDate - date < milliseconds);
    }

    function takes_long() {
      sleep(2000);
      return "hello";
    }
  )""";

  atomic<bool> done_loading(false);

  auto result = dispatcher.Broadcast(
      move(load_request),
      [&done_loading](unique_ptr<StatusOr<ResponseObject>> resp) {
        EXPECT_TRUE(resp->ok());
        done_loading.store(true);
      });
  EXPECT_SUCCESS(result);

  WaitUntil([&done_loading]() { return done_loading.load(); });

  vector<InvocationRequestStrInput> batch;
  for (int i = 0; i < 2; i++) {
    auto execute_request = InvocationRequestStrInput();
    execute_request.id = "some_id" + to_string(i);
    execute_request.version_num = 1;
    execute_request.handler_name = "takes_long";
    batch.push_back(execute_request);
  }

  atomic<bool> finished_batch(false);

  result = dispatcher.DispatchBatch(
      batch, [&finished_batch](
                 const vector<StatusOr<ResponseObject>>& batch_response) {
        for (auto& r : batch_response) {
          EXPECT_TRUE(r.ok());
        }
        finished_batch.store(true);
      });

  // This dispatch batch should work as queues were empty
  EXPECT_SUCCESS(result);

  result = dispatcher.DispatchBatch(
      batch,
      [](const vector<StatusOr<ResponseObject>>& batch_response) { return; });

  // This dispatch batch should not work as queues are not empty
  EXPECT_FALSE(result.Successful());

  WaitUntil([&finished_batch]() { return finished_batch.load(); });
}

TEST(DispatcherTest, ShouldBeAbleToExecutePreviouslyLoadedCodeAfterCrash) {
  auto async_executor = make_shared<AsyncExecutor>(1, 10);

  vector<WorkerApiSapiConfig> configs;
  WorkerApiSapiConfig config;
  config.worker_js_engine = worker::WorkerFactory::WorkerEngine::v8;
  config.js_engine_require_code_preload = true;
  config.compilation_context_cache_size = 5;
  config.native_js_function_comms_fd = -1;
  config.native_js_function_names = vector<string>();
  configs.push_back(config);

  // Only one worker in the pool
  shared_ptr<WorkerPool> worker_pool =
      make_shared<WorkerPoolApiSapi>(configs, 1);
  AutoInitRunStop for_async_executor(*async_executor);
  AutoInitRunStop for_worker_pool(*worker_pool);

  Dispatcher dispatcher(async_executor, worker_pool, 10, 5);
  AutoInitRunStop for_dispatcher(dispatcher);

  auto load_request = make_unique<CodeObject>();
  load_request->id = "some_id";
  load_request->version_num = 1;
  load_request->js =
      "function test(input) { return input + \" Some string\"; }";

  atomic<bool> done_loading(false);

  auto result = dispatcher.Dispatch(
      move(load_request),
      [&done_loading](unique_ptr<StatusOr<ResponseObject>> resp) {
        EXPECT_TRUE(resp->ok());
        done_loading.store(true);
      });
  EXPECT_SUCCESS(result);

  WaitUntil([&done_loading]() { return done_loading.load(); });

  auto execute_request = make_unique<InvocationRequestStrInput>();
  execute_request->id = "some_id";
  execute_request->version_num = 1;
  execute_request->handler_name = "test";
  execute_request->input.push_back("\"Hello\"");

  atomic<bool> done_executing(false);

  result = dispatcher.Dispatch(
      move(execute_request),
      [&done_executing](unique_ptr<StatusOr<ResponseObject>> resp) {
        EXPECT_TRUE(resp->ok());
        EXPECT_EQ("\"Hello Some string\"", (*resp)->resp);
        done_executing.store(true);
      });

  EXPECT_SUCCESS(result);

  WaitUntil([&done_executing]() { return done_executing.load(); });

  // We loaded and executed successfully, so now we kill the one worker
  auto worker = worker_pool->GetWorker(0);
  EXPECT_SUCCESS(worker.result());
  (*worker)->Terminate();

  // This coming execution we expect will fail since the worker has died. But
  // the execution flow should cause it to be restarted.

  done_executing.store(false);

  execute_request = make_unique<InvocationRequestStrInput>();
  execute_request->id = "some_id";
  execute_request->version_num = 1;
  execute_request->handler_name = "test";
  execute_request->input.push_back("\"Hello\"");

  result = dispatcher.Dispatch(
      move(execute_request),
      [&done_executing](unique_ptr<StatusOr<ResponseObject>> resp) {
        // This execution should fail since the worker has died
        EXPECT_FALSE(resp->ok());
        done_executing.store(true);
      });
  EXPECT_SUCCESS(result);

  WaitUntil([&done_executing]() { return done_executing.load(); });

  // Now we execute again an this time around we expect it to work

  done_executing.store(false);

  execute_request = make_unique<InvocationRequestStrInput>();
  execute_request->id = "some_id";
  execute_request->version_num = 1;
  execute_request->handler_name = "test";
  execute_request->input.push_back("\"Hello after restart :)\"");

  result = dispatcher.Dispatch(
      move(execute_request),
      [&done_executing](unique_ptr<StatusOr<ResponseObject>> resp) {
        EXPECT_TRUE(resp->ok());
        EXPECT_EQ("\"Hello after restart :) Some string\"", (*resp)->resp);
        done_executing.store(true);
      });

  WaitUntil([&done_executing]() { return done_executing.load(); });

  EXPECT_SUCCESS(result);
}

TEST(DispatcherTest, ShouldRecoverFromWorkerCrashWithMultipleCodeVersions) {
  auto async_executor = make_shared<AsyncExecutor>(1, 10);

  vector<WorkerApiSapiConfig> configs;
  WorkerApiSapiConfig config;
  config.worker_js_engine = worker::WorkerFactory::WorkerEngine::v8;
  config.js_engine_require_code_preload = true;
  config.compilation_context_cache_size = 5;
  config.native_js_function_comms_fd = -1;
  config.native_js_function_names = vector<string>();
  configs.push_back(config);

  // Only one worker in the pool
  shared_ptr<WorkerPool> worker_pool =
      make_shared<WorkerPoolApiSapi>(configs, 1);
  AutoInitRunStop for_async_executor(*async_executor);
  AutoInitRunStop for_worker_pool(*worker_pool);

  Dispatcher dispatcher(async_executor, worker_pool, 10, 5);
  AutoInitRunStop for_dispatcher(dispatcher);

  auto load_request = make_unique<CodeObject>();
  load_request->id = "some_id";
  load_request->version_num = 1;
  load_request->js =
      "function test(input) { return input + \" Some string 1\"; }";

  atomic<bool> done_loading(false);

  auto result = dispatcher.Dispatch(
      move(load_request),
      [&done_loading](unique_ptr<StatusOr<ResponseObject>> resp) {
        EXPECT_TRUE(resp->ok());
        done_loading.store(true);
      });
  EXPECT_SUCCESS(result);

  WaitUntil([&done_loading]() { return done_loading.load(); });

  load_request = make_unique<CodeObject>();
  load_request->id = "some_id_2";
  load_request->version_num = 2;
  load_request->js =
      "function test(input) { return input + \" Some string 2\"; }";

  done_loading.store(false);

  result = dispatcher.Dispatch(
      move(load_request),
      [&done_loading](unique_ptr<StatusOr<ResponseObject>> resp) {
        EXPECT_TRUE(resp->ok());
        done_loading.store(true);
      });
  EXPECT_SUCCESS(result);

  WaitUntil([&done_loading]() { return done_loading.load(); });

  // We kill the worker so we expect the first request right after to fail
  auto worker = worker_pool->GetWorker(0);
  EXPECT_SUCCESS(worker.result());
  (*worker)->Terminate();

  auto execute_request = make_unique<InvocationRequestStrInput>();
  execute_request->id = "some_id";
  execute_request->version_num = 1;
  execute_request->handler_name = "test";
  execute_request->input.push_back("\"Hello\"");

  atomic<bool> done_executing(false);

  result = dispatcher.Dispatch(
      move(execute_request),
      [&done_executing](unique_ptr<StatusOr<ResponseObject>> resp) {
        // This request failed but it should have caused the restart of the
        // worker so subsequent requests should work.
        EXPECT_FALSE(resp->ok());
        done_executing.store(true);
      });

  EXPECT_SUCCESS(result);

  WaitUntil([&done_executing]() { return done_executing.load(); });

  // Subsequent requests should succeed

  for (int i = 0; i < 10; i++) {
    done_executing.store(false);

    execute_request = make_unique<InvocationRequestStrInput>();
    execute_request->id = "some_id";
    execute_request->version_num = 1;
    execute_request->handler_name = "test";
    execute_request->input.push_back("\"Hello 1\"");

    result = dispatcher.Dispatch(
        move(execute_request),
        [&done_executing](unique_ptr<StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          EXPECT_EQ("\"Hello 1 Some string 1\"", (*resp)->resp);
          done_executing.store(true);
        });
    EXPECT_SUCCESS(result);

    WaitUntil([&done_executing]() { return done_executing.load(); });

    done_executing.store(false);

    execute_request = make_unique<InvocationRequestStrInput>();
    execute_request->id = "some_id_2";
    execute_request->version_num = 2;
    execute_request->handler_name = "test";
    execute_request->input.push_back("\"Hello 2\"");

    result = dispatcher.Dispatch(
        move(execute_request),
        [&done_executing](unique_ptr<StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          EXPECT_EQ("\"Hello 2 Some string 2\"", (*resp)->resp);
          done_executing.store(true);
        });

    WaitUntil([&done_executing]() { return done_executing.load(); });

    EXPECT_SUCCESS(result);
  }
}

TEST(DispatcherTest, ShouldBeAbleToLoadMoreVersionsAfterWorkerCrash) {
  auto async_executor = make_shared<AsyncExecutor>(1, 10);

  vector<WorkerApiSapiConfig> configs;
  WorkerApiSapiConfig config;
  config.worker_js_engine = worker::WorkerFactory::WorkerEngine::v8;
  config.js_engine_require_code_preload = true;
  config.compilation_context_cache_size = 5;
  config.native_js_function_comms_fd = -1;
  config.native_js_function_names = vector<string>();
  configs.push_back(config);

  // Only one worker in the pool
  shared_ptr<WorkerPool> worker_pool =
      make_shared<WorkerPoolApiSapi>(configs, 1);
  AutoInitRunStop for_async_executor(*async_executor);
  AutoInitRunStop for_worker_pool(*worker_pool);

  Dispatcher dispatcher(async_executor, worker_pool, 10, 5);
  AutoInitRunStop for_dispatcher(dispatcher);

  auto load_request = make_unique<CodeObject>();
  load_request->id = "some_id";
  load_request->version_num = 1;
  load_request->js =
      "function test(input) { return input + \" Some string 1\"; }";

  atomic<bool> done_loading(false);

  auto result = dispatcher.Dispatch(
      move(load_request),
      [&done_loading](unique_ptr<StatusOr<ResponseObject>> resp) {
        EXPECT_TRUE(resp->ok());
        done_loading.store(true);
      });
  EXPECT_SUCCESS(result);

  WaitUntil([&done_loading]() { return done_loading.load(); });

  load_request = make_unique<CodeObject>();
  load_request->id = "some_id_2";
  load_request->version_num = 2;
  load_request->js =
      "function test(input) { return input + \" Some string 2\"; }";

  done_loading.store(false);

  result = dispatcher.Dispatch(
      move(load_request),
      [&done_loading](unique_ptr<StatusOr<ResponseObject>> resp) {
        EXPECT_TRUE(resp->ok());
        done_loading.store(true);
      });
  EXPECT_SUCCESS(result);

  WaitUntil([&done_loading]() { return done_loading.load(); });

  // We kill the worker so we expect the first request right after to fail
  auto worker = worker_pool->GetWorker(0);
  EXPECT_SUCCESS(worker.result());
  (*worker)->Terminate();

  for (int i = 0; i < 2; i++) {
    // The first load should fail as the worker had died
    load_request = make_unique<CodeObject>();
    load_request->id = "some_id_3";
    load_request->version_num = 3;
    load_request->js =
        "function test(input) { return input + \" Some string 3\"; }";

    done_loading.store(false);

    result = dispatcher.Dispatch(
        move(load_request),
        [&done_loading, i](unique_ptr<StatusOr<ResponseObject>> resp) {
          if (i == 0) {
            // Failed
            EXPECT_FALSE(resp->ok());
          } else {
            EXPECT_TRUE(resp->ok());
          }

          done_loading.store(true);
        });
    EXPECT_SUCCESS(result);

    WaitUntil([&done_loading]() { return done_loading.load(); });
  }

  // Execute all versions, those loaded before and after the worker crash
  atomic<bool> done_executing(false);

  for (int i = 0; i < 10; i++) {
    done_executing.store(false);

    auto execute_request = make_unique<InvocationRequestStrInput>();
    execute_request->id = "some_id";
    execute_request->version_num = 1;
    execute_request->handler_name = "test";
    execute_request->input.push_back("\"Hello 1\"");

    result = dispatcher.Dispatch(
        move(execute_request),
        [&done_executing](unique_ptr<StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          EXPECT_EQ("\"Hello 1 Some string 1\"", (*resp)->resp);
          done_executing.store(true);
        });
    EXPECT_SUCCESS(result);

    WaitUntil([&done_executing]() { return done_executing.load(); });

    done_executing.store(false);

    execute_request = make_unique<InvocationRequestStrInput>();
    execute_request->id = "some_id_2";
    execute_request->version_num = 2;
    execute_request->handler_name = "test";
    execute_request->input.push_back("\"Hello 2\"");

    result = dispatcher.Dispatch(
        move(execute_request),
        [&done_executing](unique_ptr<StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          EXPECT_EQ("\"Hello 2 Some string 2\"", (*resp)->resp);
          done_executing.store(true);
        });

    WaitUntil([&done_executing]() { return done_executing.load(); });

    EXPECT_SUCCESS(result);

    done_executing.store(false);

    execute_request = make_unique<InvocationRequestStrInput>();
    execute_request->id = "some_id_3";
    execute_request->version_num = 3;
    execute_request->handler_name = "test";
    execute_request->input.push_back("\"Hello 3\"");

    result = dispatcher.Dispatch(
        move(execute_request),
        [&done_executing](unique_ptr<StatusOr<ResponseObject>> resp) {
          EXPECT_TRUE(resp->ok());
          EXPECT_EQ("\"Hello 3 Some string 3\"", (*resp)->resp);
          done_executing.store(true);
        });

    WaitUntil([&done_executing]() { return done_executing.load(); });

    EXPECT_SUCCESS(result);
  }
}

TEST(DispatcherTest, ShouldFailIfCodeVersionCacheSizeIsZero) {
  auto async_executor = make_shared<AsyncExecutor>(1, 10);
  constexpr size_t size = 0;
  shared_ptr<WorkerPool> worker_pool =
      make_shared<WorkerPoolApiSapi>(vector<WorkerApiSapiConfig>(), size);
  constexpr size_t max_pending_requests = 10;
  constexpr size_t code_version_cache_size = 0;

  EXPECT_DEATH(Dispatcher(async_executor, worker_pool, max_pending_requests,
                          code_version_cache_size),
               "code_version_cache_size cannot be zero");
}

TEST(DispatcherTest, ShouldFailIfMaxPendingRequestsIsZero) {
  auto async_executor = make_shared<AsyncExecutor>(1, 10);
  constexpr size_t size = 0;
  shared_ptr<WorkerPool> worker_pool =
      make_shared<WorkerPoolApiSapi>(vector<WorkerApiSapiConfig>(), size);
  constexpr size_t max_pending_requests = 0;
  constexpr size_t code_version_cache_size = 5;

  EXPECT_DEATH(Dispatcher(async_executor, worker_pool, max_pending_requests,
                          code_version_cache_size),
               "max_pending_requests cannot be zero");
}
}  // namespace google::scp::roma::sandbox::dispatcher::test
