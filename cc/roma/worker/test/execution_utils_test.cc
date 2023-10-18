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
#include <string>
#include <vector>

#include "core/test/utils/auto_init_run_stop.h"
#include "core/test/utils/conditional_wait.h"
#include "include/libplatform/libplatform.h"
#include "public/core/test/interface/execution_result_matchers.h"
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
using google::scp::core::test::ResultIs;
using google::scp::roma::WasmDataType;
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
  static void SetUpTestSuite() {
    const int my_pid = getpid();
    const string proc_exe_path = string("/proc/") + to_string(my_pid) + "/exe";
    auto my_path = std::make_unique<char[]>(PATH_MAX);
    ssize_t sz = readlink(proc_exe_path.c_str(), my_path.get(), PATH_MAX);
    ASSERT_GT(sz, 0);
    v8::V8::InitializeICUDefaultLocation(my_path.get());
    v8::V8::InitializeExternalStartupData(my_path.get());
    platform_ = v8::platform::NewDefaultPlatform().release();
    v8::V8::InitializePlatform(platform_);
    v8::V8::Initialize();
  }

  void SetUp() override {
    config.number_of_workers = 1;

    create_params_.array_buffer_allocator =
        v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    isolate_ = Isolate::New(create_params_);
  }

  void TearDown() override {
    isolate_->Dispose();
    delete create_params_.array_buffer_allocator;
  }

  struct RunCodeArguments {
    std::string js;
    std::string wasm;
    WasmDataType wasm_return_type;
    std::string handler_name;
    std::vector<std::string_view> input;
  };

  // RunCode() is used to create an executable environment to execute the code.
  ExecutionResult RunCode(const RunCodeArguments& args, std::string& output,
                          std::string& err_msg) noexcept {
    Isolate::Scope isolate_scope(isolate_);
    HandleScope handle_scope(isolate_);
    TryCatch try_catch(isolate_);
    Local<Context> context = Context::New(isolate_);
    Context::Scope context_scope(context);

    Local<Value> handler;

    // Compile and run JavaScript code object when JavaScript code obj is
    // available.
    if (!args.js.empty()) {
      auto result = ExecutionUtils::CompileRunJS(args.js, err_msg);
      if (!result.Successful()) {
        std::cout << "Error CompileRunJS:" << err_msg << "\n" << std::endl;
        return result;
      }

      // Get handler value from compiled JS code.
      auto result_get_handler =
          ExecutionUtils::GetJsHandler(args.handler_name, handler, err_msg);
      if (!result_get_handler.Successful()) {
        return result_get_handler;
      }
    }

    bool is_wasm_run = false;

    // Compile and run wasm code object when only WASM is available.
    if (args.js.empty() && !args.wasm.empty()) {
      auto result = ExecutionUtils::CompileRunWASM(args.wasm, err_msg);
      if (!result.Successful()) {
        std::cout << "Error CompileRunWASM:" << err_msg << "\n" << std::endl;
        return result;
      }

      // Get handler value from compiled JS code.
      auto result_get_handler =
          ExecutionUtils::GetWasmHandler(args.handler_name, handler, err_msg);
      if (!result_get_handler.Successful()) {
        return result_get_handler;
      }

      is_wasm_run = true;
    }

    auto input = args.input;
    auto argc = input.size();
    Local<Value> argv[argc];
    {
      auto argv_array =
          ExecutionUtils::InputToLocalArgv(input, is_wasm_run).As<Array>();
      // If argv_array size doesn't match with input. Input conversion failed.
      if (argv_array.IsEmpty() || argv_array->Length() != argc) {
        err_msg = ExecutionUtils::DescribeError(isolate_, &try_catch);
        return FailureExecutionResult(SC_ROMA_V8_WORKER_BAD_INPUT_ARGS);
      }
      for (size_t i = 0; i < argc; ++i) {
        argv[i] = argv_array->Get(context, i).ToLocalChecked();
      }
    }

    Local<Function> handler_func = handler.As<Function>();
    Local<Value> result;
    if (!handler_func->Call(context, context->Global(), argc, argv)
             .ToLocal(&result)) {
      err_msg = ExecutionUtils::DescribeError(isolate_, &try_catch);
      return FailureExecutionResult(SC_ROMA_V8_WORKER_CODE_EXECUTION_FAILURE);
    }

    // If this is a WASM run then we need to deserialize the returned data
    if (is_wasm_run) {
      auto offset = result.As<Int32>()->Value();

      result = ExecutionUtils::ReadFromWasmMemory(isolate_, context, offset,
                                                  args.wasm_return_type);
    }

    auto json_string_maybe = JSON::Stringify(context, result);
    Local<String> json_string;
    if (!json_string_maybe.ToLocal(&json_string)) {
      err_msg = ExecutionUtils::DescribeError(isolate_, &try_catch);
      return FailureExecutionResult(SC_ROMA_V8_WORKER_RESULT_PARSE_FAILURE);
    }

    String::Utf8Value result_utf8(isolate_, json_string);
    output = std::string(*result_utf8, result_utf8.length());
    return SuccessExecutionResult();
  }

  Config config;
  Isolate::CreateParams create_params_;
  static v8::Platform* platform_;
  Isolate* isolate_{nullptr};
};

v8::Platform* ExecutionUtilsTest::platform_{nullptr};

TEST_F(ExecutionUtilsTest, InputToLocalArgv) {
  vector<string> list = {"1", "2", "3"};
  {
    Isolate::Scope isolate_scope(isolate_);
    HandleScope handle_scope(isolate_);
    Local<Context> context = Context::New(isolate_);
    Context::Scope context_scope(context);
    std::vector<std::string_view> input(list.begin(), list.end());
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
  vector<string> list = {"{\"value\":1}", "{\"value\":2}"};
  {
    Isolate::Scope isolate_scope(isolate_);
    HandleScope handle_scope(isolate_);
    Local<Context> context = Context::New(isolate_);
    Context::Scope context_scope(context);

    std::vector<std::string_view> input(list.begin(), list.end());
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
  vector<string> list = {"{favoriteFruit: \"apple\"}", "{\"value\":2}"};
  {
    Isolate::Scope isolate_scope(isolate_);
    HandleScope handle_scope(isolate_);
    Local<Context> context = Context::New(isolate_);
    Context::Scope context_scope(context);

    std::vector<std::string_view> input(list.begin(), list.end());
    auto v8_array = ExecutionUtils::ExecutionUtils::InputToLocalArgv(input);
    EXPECT_TRUE(v8_array.IsEmpty());
  }
}

TEST_F(ExecutionUtilsTest, InputToLocalArgvInputWithEmptyString) {
  vector<string> list = {"", "{\"value\":2}", "{}"};
  vector<string> expected_list = {"undefined", "{\"value\":2}", "{}"};
  {
    Isolate::Scope isolate_scope(isolate_);
    HandleScope handle_scope(isolate_);
    Local<Context> context = Context::New(isolate_);
    Context::Scope context_scope(context);

    std::vector<std::string_view> input(list.begin(), list.end());
    auto v8_array = ExecutionUtils::ExecutionUtils::InputToLocalArgv(input);
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
  RunCodeArguments code_obj;
  code_obj.js =
      "function Handler(a, b) { "
      "return (a[\"value\"] + b[\"value\"]); }";
  code_obj.handler_name = "Handler";
  code_obj.input =
      std::vector<std::string_view>({"{value\":1}", "{\"value\":2}"});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_THAT(
      result,
      ResultIs(FailureExecutionResult(SC_ROMA_V8_WORKER_BAD_INPUT_ARGS)));
}

TEST_F(ExecutionUtilsTest, RunCodeObjWithJsonInput) {
  RunCodeArguments code_obj;
  code_obj.js =
      "function Handler(a, b) { "
      "return (a[\"value\"] + b[\"value\"]); }";
  code_obj.handler_name = "Handler";
  code_obj.input =
      std::vector<std::string_view>({"{\"value\":1}", "{\"value\":2}"});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(output, "3");
}

TEST_F(ExecutionUtilsTest, RunCodeObjWithJsonInputMissKey) {
  RunCodeArguments code_obj;
  code_obj.js =
      "function Handler(a, b) { "
      "return (a[\"value\"] + b[\"value\"]); }";
  code_obj.handler_name = "Handler";
  code_obj.input = std::vector<std::string_view>({"{:1}", "{\"value\":2}"});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_THAT(
      result,
      ResultIs(FailureExecutionResult(SC_ROMA_V8_WORKER_BAD_INPUT_ARGS)));
}

TEST_F(ExecutionUtilsTest, RunCodeObjWithJsonInputMissValue) {
  RunCodeArguments code_obj;
  code_obj.js =
      "function Handler(a, b) { "
      "return (a[\"value\"] + b[\"value\"]); }";
  code_obj.handler_name = "Handler";
  code_obj.input =
      std::vector<std::string_view>({"{\"value\"}", "{\"value\":2}"});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_THAT(
      result,
      ResultIs(FailureExecutionResult(SC_ROMA_V8_WORKER_BAD_INPUT_ARGS)));
}

TEST_F(ExecutionUtilsTest, RunCodeObjRunWithLessArgs) {
  // When handler function argument is Json data, function still can call and
  // run without any error, but there is no valid output.
  RunCodeArguments code_obj;
  code_obj.js =
      "function Handler(a, b) { "
      "return (a + b); }";
  code_obj.handler_name = "Handler";

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(output, "null");
}

TEST_F(ExecutionUtilsTest, RunCodeObjRunWithJsonArgsMissing) {
  // When handler function argument is Json data, empty input will cause
  // function call fail.
  RunCodeArguments code_obj;
  code_obj.js =
      "function Handler(a, b) { "
      "return (a[\"value\"] + b[\"value\"]); }";
  code_obj.handler_name = "Handler";

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_EQ(result,
            FailureExecutionResult(SC_ROMA_V8_WORKER_CODE_EXECUTION_FAILURE));
}

TEST_F(ExecutionUtilsTest, NoHandlerName) {
  RunCodeArguments code_obj;
  code_obj.js = "function Handler(a, b) {return;}";
  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_THAT(
      result,
      ResultIs(FailureExecutionResult(SC_ROMA_V8_WORKER_BAD_HANDLER_NAME)));
}

TEST_F(ExecutionUtilsTest, UnmatchedHandlerName) {
  RunCodeArguments code_obj;
  code_obj.js = "function Handler(a, b) {return;}";
  code_obj.handler_name = "Handler2";

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_EQ(result,
            FailureExecutionResult(SC_ROMA_V8_WORKER_HANDLER_INVALID_FUNCTION));
}

TEST_F(ExecutionUtilsTest, ScriptCompileFailure) {
  std::string output;
  std::string err_msg;
  std::string js("function Handler(a, b) {");
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
  RunCodeArguments code_obj;
  code_obj.handler_name = "Handler";
  code_obj.js = "function Handler() {return;}";
  code_obj.input = std::vector<std::string_view>({"1", "0"});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
}

TEST_F(ExecutionUtilsTest, CodeExecutionFailure) {
  RunCodeArguments code_obj;
  code_obj.handler_name = "Handler";
  code_obj.js = "function Handler() { throw new Error('Required'); return;}";

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_EQ(result,
            FailureExecutionResult(SC_ROMA_V8_WORKER_CODE_EXECUTION_FAILURE));
}

TEST_F(ExecutionUtilsTest, WasmSourceCode) {
  RunCodeArguments code_obj;
  code_obj.js = "";
  code_obj.wasm_return_type = WasmDataType::kUint32;
  code_obj.handler_name = "add";
  // taken from:
  // https://github.com/v8/v8/blob/5fe0aa3bc79c0a9d3ad546b79211f07105f09585/samples/hello-world.cc#L69C6-L75C12
  char wasm_bin[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01,
                     0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03,
                     0x02, 0x01, 0x00, 0x07, 0x07, 0x01, 0x03, 0x61, 0x64,
                     0x64, 0x00, 0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20,
                     0x00, 0x20, 0x01, 0x6a, 0x0b};
  code_obj.wasm.assign(wasm_bin, sizeof(wasm_bin));
  code_obj.input = std::vector<std::string_view>({"1", "2"});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(output, "3");
}

TEST_F(ExecutionUtilsTest, WasmSourceCodeCompileFailed) {
  RunCodeArguments code_obj;
  code_obj.js = "";
  code_obj.handler_name = "add";
  // Bad wasm byte string.
  char wasm_bin[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
                     0x01, 0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01};
  code_obj.wasm.assign(wasm_bin, sizeof(wasm_bin));
  code_obj.input = std::vector<std::string_view>({"1", "2"});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_EQ(result,
            FailureExecutionResult(SC_ROMA_V8_WORKER_WASM_COMPILE_FAILURE));
}

TEST_F(ExecutionUtilsTest, WasmSourceCodeUnmatchedName) {
  RunCodeArguments code_obj;
  code_obj.js = "";
  code_obj.handler_name = "plus";
  char wasm_bin[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01,
                     0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03,
                     0x02, 0x01, 0x00, 0x07, 0x07, 0x01, 0x03, 0x61, 0x64,
                     0x64, 0x00, 0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20,
                     0x00, 0x20, 0x01, 0x6a, 0x0b};
  code_obj.wasm.assign(wasm_bin, sizeof(wasm_bin));
  code_obj.input = std::vector<std::string_view>({"1", "2"});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_EQ(result,
            FailureExecutionResult(SC_ROMA_V8_WORKER_HANDLER_INVALID_FUNCTION));
  EXPECT_EQ(output, "");
}

TEST_F(ExecutionUtilsTest, CppWasmWithStringInputAndStringOutput) {
  RunCodeArguments code_obj;
  code_obj.js = "";
  code_obj.handler_name = "Handler";
  code_obj.wasm_return_type = WasmDataType::kString;

  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./cc/roma/testing/cpp_wasm_string_in_string_out_example/"
      "string_in_string_out.wasm");
  code_obj.wasm.assign(reinterpret_cast<char*>(wasm_bin.data()),
                       wasm_bin.size());
  code_obj.input = std::vector<std::string_view>({"\"Input String :)\""});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(output, "\"Input String :) Hello World from WASM\"");
}

TEST_F(ExecutionUtilsTest, RustWasmWithStringInputAndStringOutput) {
  RunCodeArguments code_obj;
  code_obj.js = "";
  code_obj.handler_name = "Handler";
  code_obj.wasm_return_type = WasmDataType::kString;

  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./cc/roma/testing/rust_wasm_string_in_string_out_example/"
      "string_in_string_out.wasm");
  code_obj.wasm.assign(reinterpret_cast<char*>(wasm_bin.data()),
                       wasm_bin.size());
  code_obj.input = std::vector<std::string_view>({"\"Input String :)\""});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(output, "\"Input String :) Hello from rust!\"");
}

TEST_F(ExecutionUtilsTest, CppWasmWithListOfStringInputAndListOfStringOutput) {
  RunCodeArguments code_obj;
  code_obj.js = "";
  code_obj.handler_name = "Handler";
  code_obj.wasm_return_type = WasmDataType::kListOfString;

  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./cc/roma/testing/cpp_wasm_list_of_string_in_list_of_string_out_example/"
      "list_of_string_in_list_of_string_out.wasm");
  code_obj.wasm.assign(reinterpret_cast<char*>(wasm_bin.data()),
                       wasm_bin.size());
  code_obj.input = std::vector<std::string_view>(
      {"[\"Input String One\", \"Input String Two\"]"});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(std::string(output.c_str(), output.length()),
            "[\"Input String One\",\"Input String Two\",\"String from "
            "Cpp1\",\"String from Cpp2\"]");
}

TEST_F(ExecutionUtilsTest, RustWasmWithListOfStringInputAndListOfStringOutput) {
  RunCodeArguments code_obj;
  code_obj.js = "";
  code_obj.handler_name = "Handler";
  code_obj.wasm_return_type = WasmDataType::kListOfString;

  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./cc/roma/testing/"
      "rust_wasm_list_of_string_in_list_of_string_out_example/"
      "list_of_string_in_list_of_string_out.wasm");
  code_obj.wasm.assign(reinterpret_cast<char*>(wasm_bin.data()),
                       wasm_bin.size());
  code_obj.input = std::vector<std::string_view>(
      {"[\"Input String One\", \"Input String Two\"]"});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(std::string(output.c_str(), output.length()),
            "[\"Input String One\",\"Input String Two\",\"Hello from "
            "rust1\",\"Hello from rust2\"]");
}

TEST_F(ExecutionUtilsTest, JsEmebbedGlobalWasmCompileRunExecute) {
  RunCodeArguments code_obj;
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
  code_obj.input = std::vector<std::string_view>({"1", "2"});

  std::string output;
  std::string err_msg;
  auto result = RunCode(code_obj, output, err_msg);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(output, to_string(3).c_str());
}
}  // namespace google::scp::roma::worker::test
