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

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "core/test/utils/conditional_wait.h"
#include "roma/interface/roma.h"
#include "roma/roma_service/src/roma_service.h"

using absl::StatusOr;
using google::scp::core::test::WaitUntil;
using std::atomic;
using std::bind;
using std::get;
using std::make_shared;
using std::make_unique;
using std::move;
using std::string;
using std::tuple;
using std::unique_ptr;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

using namespace std::chrono_literals;  // NOLINT

namespace google::scp::roma::test {

static string FunctionThatCrashes(tuple<string>& input) {
  abort();
  return "";
}

// This test registers a C++ handler which causes the worker process to die.
// The expected behavior is that the request still gets a response.
// And we should be able to load new code objects and execute again.
TEST(RomaBasicE2ETest,
     ShouldGetResponseEvenIfWorkerDiesAndShouldBeAbleToLoadAgainAndExecute) {
  // Create function binding object to add to config
  auto function_object = make_unique<FunctionBindingObject<string, string>>();
  function_object->function_name = "my_cool_func";
  function_object->function = FunctionThatCrashes;

  // Create config object and add function registration object to it
  Config config;
  config.NumberOfWorkers = 2;
  config.RegisterFunctionBinding(move(function_object));

  auto status = RomaInit(config);
  EXPECT_TRUE(status.ok());

  auto start_time = high_resolution_clock::now();

  string result;
  atomic<bool> load_finished{false};
  atomic<bool> execute_finished{false};
  {
    auto code_obj = make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_num = 1;
    code_obj->js = R"JS_CODE(
      function Handler(input) { return my_cool_func(input); }
      )JS_CODE";

    status = LoadCodeObj(move(code_obj),
                         [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
                           EXPECT_TRUE(resp->ok());
                           load_finished.store(true);
                         });
    EXPECT_TRUE(status.ok());
  }

  WaitUntil([&]() { return load_finished.load(); }, 10s);
  auto end_time = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end_time - start_time);
  std::cout << "Load took " << duration.count() << " ms." << std::endl;

  start_time = high_resolution_clock::now();

  {
    auto execution_obj = make_unique<InvocationRequestSharedInput>();
    execution_obj->id = "foo";
    execution_obj->version_num = 1;
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back(make_shared<string>("\"Foobar:\""));

    status = Execute(move(execution_obj),
                     [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
                       // The call should have failed
                       EXPECT_FALSE(resp->ok());
                       execute_finished.store(true);
                     });
    EXPECT_TRUE(status.ok());
  }

  WaitUntil([&]() { return execute_finished.load(); }, 10s);
  EXPECT_EQ(result, "");

  end_time = high_resolution_clock::now();
  duration = duration_cast<milliseconds>(end_time - start_time);
  std::cout << "Execute took " << duration.count() << " ms." << std::endl;

  load_finished.store(false);
  {
    auto code_obj = make_unique<CodeObject>();
    code_obj->id = "foo1";
    code_obj->version_num = 2;
    code_obj->js = R"JS_CODE(
      function GetVersion() { return { test : ", version, " }; }
      )JS_CODE";

    status = LoadCodeObj(move(code_obj),
                         [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
                           EXPECT_TRUE(resp->ok());
                           load_finished.store(true);
                         });
    EXPECT_TRUE(status.ok());
  }
  WaitUntil([&]() { return load_finished.load(); }, 10s);

  const int num_requests = 5;
  atomic<int> finished_runs{0};
  {
    for (int i = 0; i < num_requests; i++) {
      auto execution_obj = make_unique<InvocationRequestSharedInput>();
      execution_obj->id = "foo1";
      execution_obj->version_num = 2;
      execution_obj->handler_name = "GetVersion";

      status = Execute(move(execution_obj),
                       [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
                         EXPECT_TRUE(resp->ok());
                         auto& code_resp = **resp;
                         EXPECT_EQ(code_resp.resp, R"({"test":", version, "})");
                         finished_runs++;
                       });
      EXPECT_TRUE(status.ok());
    }
  }
  WaitUntil([&]() { return finished_runs.load() == num_requests; }, 10s);

  status = RomaStop();
  EXPECT_TRUE(status.ok());
}
}  // namespace google::scp::roma::test
