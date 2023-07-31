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

#include "roma/worker/src/execution_utils.h"

#include <gtest/gtest.h>

#include <v8-context.h>
#include <v8-initialization.h>
#include <v8-isolate.h>

#include <linux/limits.h>

#include <memory>
#include <vector>

#include "core/test/utils/auto_init_run_stop.h"
#include "core/test/utils/conditional_wait.h"
#include "include/libplatform/libplatform.h"
#include "roma/ipc/src/ipc_manager.h"
#include "roma/wasm/src/deserializer.h"
#include "roma/wasm/src/wasm_types.h"
#include "roma/wasm/test/testing_utils.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::SC_ROMA_V8_WORKER_BAD_HANDLER_NAME;
using google::scp::core::errors::SC_ROMA_V8_WORKER_BAD_INPUT_ARGS;
using google::scp::core::errors::SC_ROMA_V8_WORKER_CODE_COMPILE_FAILURE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_CODE_EXECUTION_FAILURE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_HANDLER_INVALID_FUNCTION;
using google::scp::core::errors::SC_ROMA_V8_WORKER_RESULT_PARSE_FAILURE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_WASM_COMPILE_FAILURE;
using google::scp::core::test::AutoInitRunStop;
using google::scp::roma::WasmDataType;
using google::scp::roma::common::RoleId;
using google::scp::roma::common::RomaVector;
using google::scp::roma::ipc::IpcManager;
using google::scp::roma::ipc::RomaCodeObj;
using google::scp::roma::ipc::RomaString;
using google::scp::roma::wasm::RomaWasmStringRepresentation;
using google::scp::roma::wasm::WasmDeserializer;
using google::scp::roma::wasm::testing::WasmTestingUtils;
using google::scp::roma::worker::ExecutionUtils;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;
using v8::Array;
using v8::Context;
using v8::Function;
using v8::HandleScope;
using v8::Int32;
using v8::Isolate;
using v8::JSON;
using v8::Local;
using v8::String;
using v8::TryCatch;
using v8::UnboundScript;
using v8::Value;
using v8::WasmMemoryObject;

namespace google::scp::roma::worker::test {

class ExecutionUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (!platform_) {
      int my_pid = getpid();
      string proc_exe_path = string("/proc/") + to_string(my_pid) + "/exe";
      auto my_path = std::make_unique<char[]>(PATH_MAX);
      ssize_t sz = readlink(proc_exe_path.c_str(), my_path.get(), PATH_MAX);
      ASSERT_GT(sz, 0);
      v8::V8::InitializeICUDefaultLocation(my_path.get());
      v8::V8::InitializeExternalStartupData(my_path.get());
      platform_ = v8::platform::NewDefaultPlatform();
      v8::V8::InitializePlatform(platform_.get());
      v8::V8::Initialize();
    }

    Isolate::CreateParams create_params;
    create_params.array_buffer_allocator =
        v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    isolate_ = Isolate::New(create_params);
  }

  void TearDown() override { isolate_->Dispose(); }

  // RunCode() is used to create an executable environment to execute the code.
  ExecutionResult RunCode(const RomaCodeObj& code_obj, RomaString& output,
                          RomaString& err_msg) noexcept {
    Isolate::Scope isolate_scope(isolate_);
    HandleScope handle_scope(isolate_);
    TryCatch try_catch(isolate_);
    Local<Context> context = Context::New(isolate_);
    Context::Scope context_scope(context);

    Local<Value> handler;

    // Compile and run JavaScript code object when JavaScript code obj is
    // available.
    if (!code_obj.JsIsEmpty()) {
      auto result = ExecutionUtils::CompileRunJS(code_obj.js, err_msg);
      if (!result.Successful()) {
        std::cout << "Error CompileRunJS:" << err_msg << "\n" << std::endl;
        return result;
      }

      // Get handler value from compiled JS code.
      auto result_get_handler =
          ExecutionUtils::GetJsHandler(code_obj.handler_name, handler, err_msg);
      if (!result_get_handler.Successful()) {
        return result_get_handler;
      }
    }

    bool is_wasm_run = false;

    // Compile and run wasm code object when only WASM is available.
    if (code_obj.JsIsEmpty() && !code_obj.WasmIsEmpty()) {
      auto result = ExecutionUtils::CompileRunWASM(code_obj.wasm, err_msg);
      if (!result.Successful()) {
        std::cout << "Error CompileRunWASM:" << err_msg << "\n" << std::endl;
        return result;
      }

      // Get handler value from compiled JS code.
      auto result_get_handler = ExecutionUtils::GetWasmHandler(
          code_obj.handler_name, handler, err_msg);
      if (!result_get_handler.Successful()) {
        return result_get_handler;
      }

      is_wasm_run = true;
    }

    // Parse code_obj input to args
    auto input = code_obj.input;
    auto argc = input.size();
    Local<Value> argv[argc];
    {
      auto argv_array =
          ExecutionUtils::InputToLocalArgv(input, is_wasm_run).As<Array>();
      // If argv_array size doesn't match with input. Input conversion failed.
      if (argv_array.IsEmpty() || argv_array->Length() != argc) {
        auto exception_result =
            ExecutionUtils::ReportException(&try_catch, err_msg);
        return ExecutionUtils::GetExecutionResult(
            exception_result, SC_ROMA_V8_WORKER_BAD_INPUT_ARGS);
      }
      for (size_t i = 0; i < argc; ++i) {
        argv[i] = argv_array->Get(context, i).ToLocalChecked();
      }
    }

    Local<Function> handler_func = handler.As<Function>();
    Local<Value> result;
    if (!handler_func->Call(context, context->Global(), argc, argv)
             .ToLocal(&result)) {
      auto exception_result =
          ExecutionUtils::ReportException(&try_catch, err_msg);
      return ExecutionUtils::GetExecutionResult(
          exception_result, SC_ROMA_V8_WORKER_CODE_EXECUTION_FAILURE);
    }

    // If this is a WASM run then we need to deserialize the returned data
    if (is_wasm_run) {
      auto offset = result.As<Int32>()->Value();

      result = ExecutionUtils::ReadFromWasmMemory(isolate_, context, offset,
                                                  code_obj.wasm_return_type);
    }

    auto json_string_maybe = JSON::Stringify(context, result);
    Local<String> json_string;
    if (!json_string_maybe.ToLocal(&json_string)) {
      auto exception_result =
          ExecutionUtils::ReportException(&try_catch, err_msg);
      return ExecutionUtils::GetExecutionResult(
          exception_result, SC_ROMA_V8_WORKER_RESULT_PARSE_FAILURE);
    }

    String::Utf8Value result_utf8(isolate_, json_string);
    output = RomaString(*result_utf8, result_utf8.length());
    return SuccessExecutionResult();
  }

  static unique_ptr<v8::Platform> platform_;
  Isolate* isolate_{nullptr};
};

unique_ptr<v8::Platform> ExecutionUtilsTest::platform_{nullptr};

TEST_F(ExecutionUtilsTest, RomaStrToLocalStr) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
  constexpr char test[] = "test_string";
  {
    Isolate::Scope isolate_scope(isolate_);
    HandleScope handle_scope(isolate_);
    RomaString data(test);
    Local<String> local_output;
    ExecutionUtils::RomaStrToLocalStr(data, local_output);
    String::Utf8Value output(isolate_, local_output);
    EXPECT_EQ(0, strcmp(*output, test));
  }
}

TEST_F(ExecutionUtilsTest, InputToLocalArgv) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
  vector<string> list = {"1", "2", "3"};
  {
    Isolate::Scope isolate_scope(isolate_);
    HandleScope handle_scope(isolate_);
    Local<Context> context = Context::New(isolate_);
    Context::Scope context_scope(context);
    RomaVector<RomaString> input(list.begin(), list.end());
    Local<Array> local_list = ExecutionUtils::InputToLocalArgv(input);
    for (size_t idx = 0; idx < list.size(); ++idx) {
      String::Utf8Value output(
          isolate_,
          local_list->Get(context, idx).ToLocalChecked().As<String>());
      auto expected = list.at(idx);
      EXPECT_EQ(0, strcmp(*output, expected.c_str()));
    }
  }
}

TEST_F(ExecutionUtilsTest, InputToLocalArgvJsonInput) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  vector<string> list = {"{\"value\":1}", "{\"value\":2}"};
  {
    Isolate::Scope isolate_scope(isolate_);
    HandleScope handle_scope(isolate_);
    Local<Context> context = Context::New(isolate_);
    Context::Scope context_scope(context);

    RomaVector<RomaString> input(list.begin(), list.end());
    Local<Array> local_list = ExecutionUtils::InputToLocalArgv(input);

    for (size_t idx = 0; idx < list.size(); ++idx) {
      auto expected = list.at(idx);
      auto json_value = local_list->Get(context, idx).ToLocalChecked();
      String::Utf8Value output(
          isolate_, JSON::Stringify(context, json_value).ToLocalChecked());
      EXPECT_EQ(0, strcmp(*output, expected.c_str()));
    }
  }
}

TEST_F(ExecutionUtilsTest, InputToLocalArgvInvalidJsonInput) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  vector<string> list = {"{favoriteFruit: \"apple\"}", "{\"value\":2}"};
  {
    Isolate::Scope isolate_scope(isolate_);
    HandleScope handle_scope(isolate_);
    Local<Context> context = Context::New(isolate_);
    Context::Scope context_scope(context);

    RomaVector<RomaString> input(list.begin(), list.end());
    auto v8_array = ExecutionUtils::InputToLocalArgv(input);
    EXPECT_TRUE(v8_array.IsEmpty());
  }
}

TEST_F(ExecutionUtilsTest, InputToLocalArgvInputWithEmptyString) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  vector<string> list = {"", "{\"value\":2}", "{}"};
  vector<string> expected_list = {"undefined", "{\"value\":2}", "{}"};
  {
    Isolate::Scope isolate_scope(isolate_);
    HandleScope handle_scope(isolate_);
    Local<Context> context = Context::New(isolate_);
    Context::Scope context_scope(context);

    RomaVector<RomaString> input(list.begin(), list.end());
    auto v8_array = ExecutionUtils::InputToLocalArgv(input);
    for (size_t idx = 0; idx < v8_array->Length(); ++idx) {
      auto expected = expected_list.at(idx);
      auto json_value = v8_array->Get(context, idx).ToLocalChecked();
      String::Utf8Value output(
          isolate_, JSON::Stringify(context, json_value).ToLocalChecked());
      EXPECT_EQ(0, strcmp(*output, expected.c_str()));
    }
  }
}

TEST_F(ExecutionUtilsTest, RunCodeObjWithBadInput) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  CodeObject code_obj;
  code_obj.js =
      "function Handler(a, b) { "
      "return (a[\"value\"] + b[\"value\"]); }";
  code_obj.handler_name = "Handler";
  code_obj.input =
      vector<shared_ptr<string>>({make_shared<string>("{value\":1}"),
                                  make_shared<string>("{\"value\":2}")});

  auto roma_code_obj = RomaCodeObj(code_obj);
  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_EQ(result, FailureExecutionResult(SC_ROMA_V8_WORKER_BAD_INPUT_ARGS));
}

TEST_F(ExecutionUtilsTest, RunCodeObjWithJsonInput) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  CodeObject code_obj;
  code_obj.js =
      "function Handler(a, b) { "
      "return (a[\"value\"] + b[\"value\"]); }";
  code_obj.handler_name = "Handler";
  code_obj.input =
      vector<shared_ptr<string>>({make_shared<string>("{\"value\":1}"),
                                  make_shared<string>("{\"value\":2}")});

  auto roma_code_obj = RomaCodeObj(code_obj);
  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_TRUE(result.Successful());
  EXPECT_EQ(output, "3");
}

TEST_F(ExecutionUtilsTest, RunCodeObjWithJsonInputMissKey) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  CodeObject code_obj;
  code_obj.js =
      "function Handler(a, b) { "
      "return (a[\"value\"] + b[\"value\"]); }";
  code_obj.handler_name = "Handler";
  code_obj.input = vector<shared_ptr<string>>(
      {make_shared<string>("{:1}"), make_shared<string>("{\"value\":2}")});

  auto roma_code_obj = RomaCodeObj(code_obj);
  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_EQ(result, FailureExecutionResult(SC_ROMA_V8_WORKER_BAD_INPUT_ARGS));
}

TEST_F(ExecutionUtilsTest, RunCodeObjWithJsonInputMissValue) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  CodeObject code_obj;
  code_obj.js =
      "function Handler(a, b) { "
      "return (a[\"value\"] + b[\"value\"]); }";
  code_obj.handler_name = "Handler";
  code_obj.input =
      vector<shared_ptr<string>>({make_shared<string>("{\"value\"}"),
                                  make_shared<string>("{\"value\":2}")});

  auto roma_code_obj = RomaCodeObj(code_obj);
  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_EQ(result, FailureExecutionResult(SC_ROMA_V8_WORKER_BAD_INPUT_ARGS));
}

TEST_F(ExecutionUtilsTest, RunCodeObjRunWithLessArgs) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  // When handler function argument is Json data, function still can call and
  // run without any error, but there is no valid output.
  CodeObject code_obj;
  code_obj.js =
      "function Handler(a, b) { "
      "return (a + b); }";
  code_obj.handler_name = "Handler";

  auto roma_code_obj = RomaCodeObj(code_obj);
  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_TRUE(result.Successful());
  EXPECT_EQ(output, "null");
}

TEST_F(ExecutionUtilsTest, RunCodeObjRunWithJsonArgsMissing) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  // When handler function argument is Json data, empty input will cause
  // function call fail.
  CodeObject code_obj;
  code_obj.js =
      "function Handler(a, b) { "
      "return (a[\"value\"] + b[\"value\"]); }";
  code_obj.handler_name = "Handler";

  auto roma_code_obj = RomaCodeObj(code_obj);
  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_EQ(result,
            FailureExecutionResult(SC_ROMA_V8_WORKER_CODE_EXECUTION_FAILURE));
}

TEST_F(ExecutionUtilsTest, NoHandlerName) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  CodeObject code_obj;
  code_obj.js = "function Handler(a, b) {return;}";
  auto roma_code_obj = RomaCodeObj(code_obj);
  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_EQ(result, FailureExecutionResult(SC_ROMA_V8_WORKER_BAD_HANDLER_NAME));
}

TEST_F(ExecutionUtilsTest, UnmatchedHandlerName) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  CodeObject code_obj;
  code_obj.js = "function Handler(a, b) {return;}";
  code_obj.handler_name = "Handler2";

  auto roma_code_obj = RomaCodeObj(code_obj);
  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_EQ(result,
            FailureExecutionResult(SC_ROMA_V8_WORKER_HANDLER_INVALID_FUNCTION));
}

TEST_F(ExecutionUtilsTest, ScriptCompileFailure) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
  RomaString output;
  RomaString err_msg;
  RomaString js("function Handler(a, b) {");
  {
    Isolate::Scope isolate_scope(isolate_);
    HandleScope handle_scope(isolate_);
    Local<Context> context = Context::New(isolate_);
    Context::Scope context_scope(context);
    auto result = ExecutionUtils::CompileRunJS(js, err_msg);
    EXPECT_EQ(result,
              FailureExecutionResult(SC_ROMA_V8_WORKER_CODE_COMPILE_FAILURE));
  }
}

TEST_F(ExecutionUtilsTest, SuccessWithUnNeedArgs) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  CodeObject code_obj;
  code_obj.handler_name = "Handler";
  code_obj.js = "function Handler() {return;}";
  code_obj.input = vector<shared_ptr<string>>(
      {make_shared<string>("1"), make_shared<string>("0")});

  auto roma_code_obj = RomaCodeObj(code_obj);
  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_EQ(result, SuccessExecutionResult());
}

TEST_F(ExecutionUtilsTest, CodeExecutionFailure) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  CodeObject code_obj;
  code_obj.handler_name = "Handler";
  code_obj.js = "function Handler() { throw new Error('Required'); return;}";
  code_obj.input = vector<shared_ptr<string>>({});

  auto roma_code_obj = RomaCodeObj(code_obj);
  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_EQ(result,
            FailureExecutionResult(SC_ROMA_V8_WORKER_CODE_EXECUTION_FAILURE));
}

TEST_F(ExecutionUtilsTest, WasmSourceCode) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  CodeObject code_obj;
  code_obj.js = "";
  code_obj.wasm_return_type = WasmDataType::kUint32;
  code_obj.handler_name = "add";
  // taken from: https://github.com/v8/v8/blob/master/samples/hello-world.cc#L66
  char wasm_bin[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01,
                     0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03,
                     0x02, 0x01, 0x00, 0x07, 0x07, 0x01, 0x03, 0x61, 0x64,
                     0x64, 0x00, 0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20,
                     0x00, 0x20, 0x01, 0x6a, 0x0b};
  code_obj.wasm.assign(wasm_bin, sizeof(wasm_bin));
  code_obj.input = vector<shared_ptr<string>>(
      {make_shared<string>("1"), make_shared<string>("2")});
  auto roma_code_obj = RomaCodeObj(code_obj);

  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_TRUE(result.Successful());
  EXPECT_EQ(output, "3");
}

TEST_F(ExecutionUtilsTest, WasmSourceCodeCompileFailed) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  CodeObject code_obj;
  code_obj.js = "";
  code_obj.handler_name = "add";
  // Bad wasm byte string.
  char wasm_bin[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
                     0x01, 0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01};
  code_obj.wasm.assign(wasm_bin, sizeof(wasm_bin));
  code_obj.input = vector<shared_ptr<string>>(
      {make_shared<string>("1"), make_shared<string>("2")});

  auto roma_code_obj = RomaCodeObj(code_obj);
  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_EQ(result,
            FailureExecutionResult(SC_ROMA_V8_WORKER_WASM_COMPILE_FAILURE));
}

TEST_F(ExecutionUtilsTest, WasmSourceCodeUnmatchedName) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  CodeObject code_obj;
  code_obj.js = "";
  code_obj.handler_name = "plus";
  char wasm_bin[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01,
                     0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03,
                     0x02, 0x01, 0x00, 0x07, 0x07, 0x01, 0x03, 0x61, 0x64,
                     0x64, 0x00, 0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20,
                     0x00, 0x20, 0x01, 0x6a, 0x0b};
  code_obj.wasm.assign(wasm_bin, sizeof(wasm_bin));
  code_obj.input = vector<shared_ptr<string>>(
      {make_shared<string>("1"), make_shared<string>("2")});
  auto roma_code_obj = RomaCodeObj(code_obj);

  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_EQ(result,
            FailureExecutionResult(SC_ROMA_V8_WORKER_HANDLER_INVALID_FUNCTION));
  EXPECT_EQ(output, "");
}

TEST_F(ExecutionUtilsTest, CppWasmWithStringInputAndStringOutput) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  CodeObject code_obj;
  code_obj.js = "";
  code_obj.handler_name = "Handler";
  code_obj.wasm_return_type = WasmDataType::kString;

  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./cc/roma/testing/cpp_wasm_string_in_string_out_example/"
      "string_in_string_out.wasm");
  code_obj.wasm.assign(reinterpret_cast<char*>(wasm_bin.data()),
                       wasm_bin.size());
  code_obj.input =
      vector<shared_ptr<string>>({make_shared<string>("\"Input String :)\"")});
  auto roma_code_obj = RomaCodeObj(code_obj);

  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_EQ(result, SuccessExecutionResult());
  EXPECT_EQ(output, "\"Input String :) Hello World from WASM\"");
}

TEST_F(ExecutionUtilsTest, RustWasmWithStringInputAndStringOutput) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  CodeObject code_obj;
  code_obj.js = "";
  code_obj.handler_name = "Handler";
  code_obj.wasm_return_type = WasmDataType::kString;

  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./cc/roma/testing/rust_wasm_string_in_string_out_example/"
      "string_in_string_out.wasm");
  code_obj.wasm.assign(reinterpret_cast<char*>(wasm_bin.data()),
                       wasm_bin.size());
  code_obj.input =
      vector<shared_ptr<string>>({make_shared<string>("\"Input String :)\"")});
  auto roma_code_obj = RomaCodeObj(code_obj);

  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_EQ(result, SuccessExecutionResult());
  EXPECT_EQ(output, "\"Input String :) Hello from rust!\"");
}

TEST_F(ExecutionUtilsTest, CppWasmWithListOfStringInputAndListOfStringOutput) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  CodeObject code_obj;
  code_obj.js = "";
  code_obj.handler_name = "Handler";
  code_obj.wasm_return_type = WasmDataType::kListOfString;

  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./cc/roma/testing/cpp_wasm_list_of_string_in_list_of_string_out_example/"
      "list_of_string_in_list_of_string_out.wasm");
  code_obj.wasm.assign(reinterpret_cast<char*>(wasm_bin.data()),
                       wasm_bin.size());
  code_obj.input = vector<shared_ptr<string>>(
      {make_shared<string>("[\"Input String One\", \"Input String Two\"]")});
  auto roma_code_obj = RomaCodeObj(code_obj);

  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_EQ(result, SuccessExecutionResult());
  EXPECT_EQ(std::string(output.c_str(), output.length()),
            "[\"Input String One\",\"Input String Two\",\"String from "
            "Cpp1\",\"String from Cpp2\"]");
}

TEST_F(ExecutionUtilsTest, RustWasmWithListOfStringInputAndListOfStringOutput) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  CodeObject code_obj;
  code_obj.js = "";
  code_obj.handler_name = "Handler";
  code_obj.wasm_return_type = WasmDataType::kListOfString;

  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./cc/roma/testing/"
      "rust_wasm_list_of_string_in_list_of_string_out_example/"
      "list_of_string_in_list_of_string_out.wasm");
  code_obj.wasm.assign(reinterpret_cast<char*>(wasm_bin.data()),
                       wasm_bin.size());
  code_obj.input = vector<shared_ptr<string>>(
      {make_shared<string>("[\"Input String One\", \"Input String Two\"]")});
  auto roma_code_obj = RomaCodeObj(code_obj);

  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_EQ(result, SuccessExecutionResult());
  EXPECT_EQ(std::string(output.c_str(), output.length()),
            "[\"Input String One\",\"Input String Two\",\"Hello from "
            "rust1\",\"Hello from rust2\"]");
}

TEST_F(ExecutionUtilsTest, JsEmebbedGlobalWasmCompileRunExecute) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

  CodeObject code_obj;
  string js = R"(
          let bytes = new Uint8Array([
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
            0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
            0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
            0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b
          ]);
          let module = new WebAssembly.Module(bytes);
          let instance = new WebAssembly.Instance(module);
          function Handler(a, b) {
          return instance.exports.add(a, b);
          }
        )";
  code_obj.js = js;
  code_obj.handler_name = "Handler";
  code_obj.input = vector<shared_ptr<string>>(
      {make_shared<string>("1"), make_shared<string>("2")});

  auto roma_code_obj = RomaCodeObj(code_obj);

  RomaString output;
  RomaString err_msg;
  auto result = RunCode(roma_code_obj, output, err_msg);
  EXPECT_EQ(result, SuccessExecutionResult());
  EXPECT_EQ(output, to_string(3).c_str());
}
}  // namespace google::scp::roma::worker::test
