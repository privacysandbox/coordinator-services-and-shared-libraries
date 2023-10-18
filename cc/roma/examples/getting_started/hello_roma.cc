// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// A minimal "hello world" style example of how to use Roma. This
// example does not include any error handling and will simply abort if
// something goes wrong.

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "cc/roma/interface/roma.h"

using google::scp::roma::CodeObject;
using google::scp::roma::Config;
using google::scp::roma::Execute;
using google::scp::roma::FunctionBindingObjectV2;
using google::scp::roma::InvocationRequestStrInput;
using google::scp::roma::LoadCodeObj;
using google::scp::roma::ResponseObject;
using google::scp::roma::RomaInit;
using google::scp::roma::RomaStop;
using google::scp::roma::proto::FunctionBindingIoProto;
using std::atomic;
using std::make_unique;
using std::move;
using std::string;
using std::unique_ptr;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

#define WAIT_UNTIL(var, timeout_ms)              \
  {                                              \
    size_t timeout_count = 1;                    \
    while (timeout_count < timeout_ms && !var) { \
      sleep_for(milliseconds(1));                \
      timeout_count++;                           \
    }                                            \
    CHECK(var);                                  \
  }

// Simple load and execute flow.
static void BasicExecution() {
  auto status = RomaInit();
  CHECK(status.ok());

  string result;
  atomic<bool> load_finished = false;
  atomic<bool> execute_finished = false;

  LOG(INFO) << "Loading code";

  {
    auto code_obj = make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_num = 1;
    code_obj->js = R"JS_CODE(
    function Handler(input) { return "hello" + input;}
  )JS_CODE";

    auto status = LoadCodeObj(
        move(code_obj), [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          CHECK(resp->ok());
          load_finished.store(true);
        });
    CHECK(status.ok());
  }

  WAIT_UNTIL(load_finished, 100);

  LOG(INFO) << "Executing code";

  {
    auto execution_obj = make_unique<InvocationRequestStrInput>();
    execution_obj->id = "foo";
    execution_obj->version_num = 1;
    execution_obj->handler_name = "Handler";
    execution_obj->input.push_back("\" world\"");

    auto status = Execute(move(execution_obj),
                          [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
                            CHECK(resp->ok());
                            if (resp->ok()) {
                              auto& code_resp = **resp;
                              result = code_resp.resp;
                            }
                            execute_finished.store(true);
                          });
    CHECK(status.ok());
  }

  WAIT_UNTIL(execute_finished, 100);

  CHECK_EQ("\"hello world\"", result);

  status = RomaStop();
  CHECK(status.ok());
}

static int g_var_to_update_in_binding = 0;

static void StrFunction(FunctionBindingIoProto& io) {
  LOG(INFO) << "Calling " << __func__;

  CHECK(io.has_input_string());
  string input = io.input_string();

  io.set_output_string(input + " from C++");

  g_var_to_update_in_binding++;
}

static void ListStrFunction(FunctionBindingIoProto& io) {
  LOG(INFO) << "Calling " << __func__;

  CHECK(io.has_input_list_of_string());
  auto input = io.input_list_of_string().data();

  for (auto& str : input) {
    io.mutable_output_list_of_string()->add_data(str);
  }
  io.mutable_output_list_of_string()->add_data("from C++");

  g_var_to_update_in_binding++;
}

static void MapStrFunction(FunctionBindingIoProto& io) {
  LOG(INFO) << "Calling " << __func__;

  CHECK(io.has_input_map_of_string());
  auto input = io.input_map_of_string().data();

  for (auto& kv : input) {
    (*io.mutable_output_map_of_string()->mutable_data())[kv.first] = kv.second;
  }
  (*io.mutable_output_map_of_string()->mutable_data())["hello"] = "from C++";

  g_var_to_update_in_binding++;
}

// This is a simple load and execute flow, but it registers C++ bindings.
// It also executes multiple code versions.
static void ExecutionWithBindings() {
  Config config;

  auto function_binding_object = make_unique<FunctionBindingObjectV2>();
  function_binding_object->function = StrFunction;
  function_binding_object->function_name = "str_function";
  config.RegisterFunctionBinding(move(function_binding_object));

  function_binding_object = make_unique<FunctionBindingObjectV2>();
  function_binding_object->function = ListStrFunction;
  function_binding_object->function_name = "list_str_function";
  config.RegisterFunctionBinding(move(function_binding_object));

  function_binding_object = make_unique<FunctionBindingObjectV2>();
  function_binding_object->function = MapStrFunction;
  function_binding_object->function_name = "map_str_function";
  config.RegisterFunctionBinding(move(function_binding_object));

  auto status = RomaInit(config);
  CHECK(status.ok());

  string result;
  atomic<bool> load_finished = false;
  atomic<bool> execute_finished = false;

  LOG(INFO) << "Loading code v1";
  // Load v1 of the code
  {
    auto code_obj = make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_num = 1;
    code_obj->js = R"JS_CODE(
    function Handler(input) { return str_function(input);}
  )JS_CODE";

    auto status = LoadCodeObj(
        move(code_obj), [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          CHECK(resp->ok());
          load_finished.store(true);
        });
    CHECK(status.ok());
  }

  WAIT_UNTIL(load_finished, 100);
  load_finished = false;

  LOG(INFO) << "Loading code v2";
  // Load v2 of the code
  {
    auto code_obj = make_unique<CodeObject>();
    code_obj->id = "foo2";
    code_obj->version_num = 2;
    code_obj->js = R"JS_CODE(
    function Handler(input) {
      list = [];
      list.push(input);
      return list_str_function(list);
      }
  )JS_CODE";

    auto status = LoadCodeObj(
        move(code_obj), [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          CHECK(resp->ok());
          load_finished.store(true);
        });
    CHECK(status.ok());
  }

  WAIT_UNTIL(load_finished, 100);
  load_finished = false;

  LOG(INFO) << "Loading code v3";
  // Load v3 of the code
  {
    auto code_obj = make_unique<CodeObject>();
    code_obj->id = "foo3";
    code_obj->version_num = 3;
    code_obj->js = R"JS_CODE(
    function Handler(input) {
      map_input = new Map();
      map_input.set("a key", input);
      map_output = map_str_function(map_input);

      if (!map_output.has("a key") ||
        !map_output.has("hello") ||
        map_output.get("a key") != input ||
        map_output.get("hello") != "from C++") {
        return "Didn't work :(";
      }

      return "Worked!";
    }
  )JS_CODE";

    auto status = LoadCodeObj(
        move(code_obj), [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
          CHECK(resp->ok());
          load_finished.store(true);
        });

    WAIT_UNTIL(load_finished, 100);

    LOG(INFO) << "Execution code v1";
    // Execute v1 of the code
    {
      auto execution_obj = make_unique<InvocationRequestStrInput>();
      execution_obj->id = "foo_exec1";
      execution_obj->version_num = 1;
      execution_obj->handler_name = "Handler";
      execution_obj->input.push_back("\"a string\"");

      auto status =
          Execute(move(execution_obj),
                  [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
                    CHECK(resp->ok());
                    if (resp->ok()) {
                      auto& code_resp = **resp;
                      result = code_resp.resp;
                    }
                    execute_finished.store(true);
                  });
      CHECK(status.ok());
    }

    WAIT_UNTIL(execute_finished, 100);
    CHECK_EQ("\"a string from C++\"", result);

    execute_finished = false;

    LOG(INFO) << "Execution code v2";
    // Execute v2 of the code
    {
      auto execution_obj = make_unique<InvocationRequestStrInput>();
      execution_obj->id = "foo_exec2";
      execution_obj->version_num = 2;
      execution_obj->handler_name = "Handler";
      execution_obj->input.push_back("\"a string\"");

      auto status =
          Execute(move(execution_obj),
                  [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
                    CHECK(resp->ok());
                    if (resp->ok()) {
                      auto& code_resp = **resp;
                      result = code_resp.resp;
                    }
                    execute_finished.store(true);
                  });
      CHECK(status.ok());
    }

    WAIT_UNTIL(execute_finished, 100);
    CHECK_EQ("[\"a string\",\"from C++\"]", result);

    execute_finished = false;

    LOG(INFO) << "Execution code v3";
    // Execute v3 of the code
    {
      auto execution_obj = make_unique<InvocationRequestStrInput>();
      execution_obj->id = "foo_exec3";
      execution_obj->version_num = 3;
      execution_obj->handler_name = "Handler";
      execution_obj->input.push_back("\"a string\"");

      auto status =
          Execute(move(execution_obj),
                  [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
                    CHECK(resp->ok());
                    if (resp->ok()) {
                      auto& code_resp = **resp;
                      result = code_resp.resp;
                    }
                    execute_finished.store(true);
                  });
      CHECK(status.ok());
    }

    WAIT_UNTIL(execute_finished, 100);
    CHECK_EQ("\"Worked!\"", result);

    // Should have been increased to 3 since we increase it in each binding
    CHECK_EQ(3, g_var_to_update_in_binding);

    status = RomaStop();
    CHECK(status.ok());
  }
}

int main() {
  LOG(INFO) << "Stating";

  BasicExecution();

  ExecutionWithBindings();

  LOG(INFO) << "Done :)";
}
