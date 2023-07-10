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

#include "roma/worker/src/execution_manager.h"

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

using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::SC_ROMA_V8_WORKER_BAD_INPUT_ARGS;
using google::scp::core::errors::SC_ROMA_V8_WORKER_CODE_EXECUTION_FAILURE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_UNKNOWN_WASM_RETURN_TYPE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_UNMATCHED_CODE_VERSION_NUM;
using google::scp::core::errors::
    SC_ROMA_V8_WORKER_UNSET_ISOLATE_WITH_PRELOADED_CODE;
using google::scp::core::test::AutoInitRunStop;
using google::scp::roma::common::RoleId;
using google::scp::roma::ipc::IpcManager;
using google::scp::roma::ipc::RomaCodeObj;
using google::scp::roma::ipc::RomaString;
using google::scp::roma::worker::ExecutionManager;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;
using v8::HandleScope;
using v8::Isolate;

namespace google::scp::roma::worker::test {
static constexpr char WasmUnCompilableError[] =
    "8: ReferenceError: WebAssembly is not defined";

class ExecutionManagerTest : public ::testing::Test {
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
  }

  void GetCodeObj(CodeObject& code_obj, string js = std::string(),
                  uint64_t version_num = 0) {
    code_obj.id = "id";
    code_obj.version_num = version_num;
    code_obj.js = js;
  }

  void GetExecutionObj(InvocationRequestSharedInput& ext_obj,
                       vector<string>& input, uint64_t version_num = 0) {
    ext_obj.id = "id";
    ext_obj.handler_name = "Handler";
    for (auto item : input) {
      ext_obj.input.push_back(make_shared<string>(item));
    }
    ext_obj.version_num = version_num;
  }

  v8::Isolate* isolate_{nullptr};
  static unique_ptr<v8::Platform> platform_;
};

unique_ptr<v8::Platform> ExecutionManagerTest::platform_{nullptr};

TEST_F(ExecutionManagerTest, ProcessJsCodeMixedWithGlobalWebAssembly) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
  ExecutionManager helper;

  // JS code has global WebAssembly variable which is updated by Handler.
  string js = R"(
          let bytes = new Uint8Array([
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
            0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
            0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
            0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b
          ]);
          let module = new WebAssembly.Module(bytes);
          let c = 1;
          let instance = new WebAssembly.Instance(module);
          function Handler(a) {
          return instance.exports.add(a, c);
          }
        )";

  // Creates an UnboundScript for JS code.
  {
    CodeObject code_obj;
    GetCodeObj(code_obj, js);
    auto roma_code_obj = RomaCodeObj(code_obj);

    RomaString err_msg;
    vector<shared_ptr<FunctionBindingObjectBase>> function_bindings;

    intptr_t external_refs[] = {0};
    auto result =
        helper.Create(roma_code_obj, err_msg, function_bindings, external_refs);
    EXPECT_EQ(result, SuccessExecutionResult())
        << GetErrorMessage(result.status_code);
    EXPECT_EQ(err_msg, WasmUnCompilableError);
  }

  // Process the code with UnboundScript created above.
  {
    for (int i = 0; i < 3; i++) {
      InvocationRequestSharedInput ext_obj;
      auto input = vector<string>({to_string(i)});
      // Here the code_obj doesn't have source code. The requests will use the
      // UnboundScript to execute request.
      GetExecutionObj(ext_obj, input);

      auto roma_code_obj = RomaCodeObj(ext_obj);

      RomaString output;
      RomaString err_msg;
      auto result = helper.Process(roma_code_obj, output, err_msg);
      EXPECT_EQ(result, SuccessExecutionResult())
          << GetErrorMessage(result.status_code);
      auto expected = to_string(i + 1);
      EXPECT_EQ(output, expected.c_str());
    }
  }

  EXPECT_EQ(helper.Stop(), SuccessExecutionResult());
}

TEST_F(ExecutionManagerTest, CreateAndProcessWasmCode) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
  ExecutionManager helper;
  // taken from:
  // https://github.com/v8/v8/blob/master/samples/hello-world.cc#L66
  char wasm_bin[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01,
                     0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03,
                     0x02, 0x01, 0x00, 0x07, 0x07, 0x01, 0x03, 0x61, 0x64,
                     0x64, 0x00, 0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20,
                     0x00, 0x20, 0x01, 0x6a, 0x0b};

  // Cache wasm source code in execution_manager.
  {
    CodeObject code_obj;
    GetCodeObj(code_obj);
    code_obj.wasm.assign(wasm_bin, sizeof(wasm_bin));
    auto roma_code_obj = RomaCodeObj(code_obj);

    RomaString err_msg;
    vector<shared_ptr<FunctionBindingObjectBase>> function_bindings;

    intptr_t external_refs[] = {0};
    auto result =
        helper.Create(roma_code_obj, err_msg, function_bindings, external_refs);
    EXPECT_EQ(result, SuccessExecutionResult());
  }

  // Process the code.
  {
    for (int i = 0; i < 3; i++) {
      InvocationRequestSharedInput ext_obj;
      auto input = vector<string>({to_string(i), to_string(i)});
      GetExecutionObj(ext_obj, input);
      ext_obj.handler_name = "add";
      ext_obj.wasm_return_type = WasmDataType::kUint32;
      auto roma_code_obj = RomaCodeObj(ext_obj);

      RomaString output;
      RomaString err_msg;
      auto result = helper.Process(roma_code_obj, output, err_msg);
      EXPECT_EQ(result, SuccessExecutionResult());
      auto expected = to_string(i * 2);
      EXPECT_EQ(output, expected.c_str());
    }
  }

  EXPECT_EQ(helper.Stop(), SuccessExecutionResult());
}

TEST_F(ExecutionManagerTest, UnknownWasmReturnType) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
  ExecutionManager helper;
  // taken from:
  // https://github.com/v8/v8/blob/master/samples/hello-world.cc#L66
  char wasm_bin[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01,
                     0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03,
                     0x02, 0x01, 0x00, 0x07, 0x07, 0x01, 0x03, 0x61, 0x64,
                     0x64, 0x00, 0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20,
                     0x00, 0x20, 0x01, 0x6a, 0x0b};

  // Cache wasm source code in execution_manager.
  {
    CodeObject code_obj;
    GetCodeObj(code_obj);
    code_obj.wasm.assign(wasm_bin, sizeof(wasm_bin));
    auto roma_code_obj = RomaCodeObj(code_obj);

    RomaString err_msg;
    vector<shared_ptr<FunctionBindingObjectBase>> function_bindings;

    intptr_t external_refs[] = {0};
    auto result =
        helper.Create(roma_code_obj, err_msg, function_bindings, external_refs);
    EXPECT_EQ(result, SuccessExecutionResult());
  }

  // Process the code.
  {
    for (int i = 0; i < 3; i++) {
      InvocationRequestSharedInput ext_obj;
      auto input = vector<string>({to_string(i), to_string(i)});
      GetExecutionObj(ext_obj, input);
      ext_obj.handler_name = "add";

      if (i % 2 == 0) {
        ext_obj.wasm_return_type = WasmDataType::kUint32;
      } else {
        ext_obj.wasm_return_type = WasmDataType::kUnknownType;
      }

      auto roma_code_obj = RomaCodeObj(ext_obj);

      RomaString output;
      RomaString err_msg;
      auto result = helper.Process(roma_code_obj, output, err_msg);
      auto expected = to_string(i * 2);
      if (i % 2 == 0) {
        EXPECT_EQ(result, SuccessExecutionResult());
        EXPECT_EQ(output, expected.c_str());
      } else {
        EXPECT_EQ(result, FailureExecutionResult(
                              SC_ROMA_V8_WORKER_UNKNOWN_WASM_RETURN_TYPE));
        std::cout << GetErrorMessage(result.status_code) << std::endl;
      }
    }
  }

  EXPECT_EQ(helper.Stop(), SuccessExecutionResult());
}

TEST_F(ExecutionManagerTest, CreateBlobAndProcessJsMixedWithLocalWebAssembly) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
  ExecutionManager helper;

  // Creates a blob.
  {
    CodeObject code_obj;
    // JS code has local WebAssembly variable.
    string js = R"(
          let bytes = new Uint8Array([
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
            0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
            0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
            0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b
          ]);
          function Handler(a, b) {
          var module = new WebAssembly.Module(bytes);
          var instance = new WebAssembly.Instance(module);
          return instance.exports.add(a, b);
          }
        )";
    GetCodeObj(code_obj, js);
    auto roma_code_obj = RomaCodeObj(code_obj);

    RomaString err_msg;
    vector<shared_ptr<FunctionBindingObjectBase>> function_bindings;

    intptr_t external_refs[] = {0};
    auto result =
        helper.Create(roma_code_obj, err_msg, function_bindings, external_refs);
    EXPECT_EQ(result, SuccessExecutionResult());
  }

  // Process the code.
  {
    for (int i = 0; i < 3; i++) {
      InvocationRequestSharedInput ext_obj;
      auto input = vector<string>({to_string(i), to_string(i)});
      GetExecutionObj(ext_obj, input);
      auto roma_code_obj = RomaCodeObj(ext_obj);

      RomaString output;
      RomaString err_msg;
      auto result = helper.Process(roma_code_obj, output, err_msg);
      EXPECT_EQ(result, SuccessExecutionResult());
      auto expected = to_string(i * 2);
      EXPECT_EQ(output, expected.c_str());
    }
  }

  EXPECT_EQ(helper.Stop(), SuccessExecutionResult());
}

TEST_F(ExecutionManagerTest, DescribeThrowError) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
  ExecutionManager helper;

  // Creates a blob success.
  {
    CodeObject code_obj;
    // JS code has global variable which is updated by Handler.
    string js = R"(
      function Handler(a, b) { throw new TypeError(); })";
    GetCodeObj(code_obj, js);
    auto roma_code_obj = RomaCodeObj(code_obj);

    RomaString err_msg;
    vector<shared_ptr<FunctionBindingObjectBase>> function_bindings;

    intptr_t external_refs[] = {0};
    auto result =
        helper.Create(roma_code_obj, err_msg, function_bindings, external_refs);
    EXPECT_EQ(result, SuccessExecutionResult());
  }

  // Process the code with error throw.
  {
    for (int i = 0; i < 3; i++) {
      InvocationRequestSharedInput ext_obj;
      auto input = vector<string>({to_string(i), to_string(i)});
      GetExecutionObj(ext_obj, input);
      auto roma_code_obj = RomaCodeObj(ext_obj);

      RomaString output;
      RomaString err_msg;
      auto result = helper.Process(roma_code_obj, output, err_msg);
      EXPECT_EQ(result, FailureExecutionResult(
                            SC_ROMA_V8_WORKER_CODE_EXECUTION_FAILURE));
      EXPECT_EQ(err_msg, "2: TypeError");
    }
  }

  EXPECT_EQ(helper.Stop(), SuccessExecutionResult());
}

TEST_F(ExecutionManagerTest, CreateBlobAndProcessJsCode) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
  ExecutionManager helper;

  // Creates a blob.
  {
    CodeObject code_obj;
    // JS code has global variable which is updated by Handler.
    string js =
        "let carNum = 0;"
        "function Handler(a, b) { carNum = carNum + 1;"
        "return (a + b + carNum - 1); }";
    GetCodeObj(code_obj, js);
    auto roma_code_obj = RomaCodeObj(code_obj);

    RomaString err_msg;
    vector<shared_ptr<FunctionBindingObjectBase>> function_bindings;

    intptr_t external_refs[] = {0};
    auto result =
        helper.Create(roma_code_obj, err_msg, function_bindings, external_refs);
    EXPECT_EQ(result, SuccessExecutionResult());
  }

  // Process the code.
  {
    for (int i = 0; i < 3; i++) {
      InvocationRequestSharedInput ext_obj;
      auto input = vector<string>({to_string(i), to_string(i)});
      GetExecutionObj(ext_obj, input);
      auto roma_code_obj = RomaCodeObj(ext_obj);

      RomaString output;
      RomaString err_msg;
      auto result = helper.Process(roma_code_obj, output, err_msg);
      EXPECT_EQ(result, SuccessExecutionResult());
      auto expected = to_string(i * 2);
      EXPECT_EQ(output, expected.c_str());
    }
  }

  EXPECT_EQ(helper.Stop(), SuccessExecutionResult());
}

TEST_F(ExecutionManagerTest, ProcessJsCodeWithInvalidInput) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
  ExecutionManager helper;

  // Creates a blob.
  {
    CodeObject code_obj;
    // JS code has global variable which is updated by Handler.
    string js =
        "let carNum = 0;"
        "function Handler(a, b) { carNum = carNum + 1;"
        "return (a + b + carNum - 1); }";
    GetCodeObj(code_obj, js);
    auto roma_code_obj = RomaCodeObj(code_obj);

    RomaString err_msg;
    vector<shared_ptr<FunctionBindingObjectBase>> function_bindings;

    intptr_t external_refs[] = {0};
    auto result =
        helper.Create(roma_code_obj, err_msg, function_bindings, external_refs);
    EXPECT_EQ(result, SuccessExecutionResult());
  }

  // Unit test for invalid input. If there is invalid string in input, the input
  // parse will fail and request process will return corresponding failure
  // result.
  {
    InvocationRequestSharedInput code_obj;
    auto input = vector<string>({"value\"", "2"});
    GetExecutionObj(code_obj, input);
    auto roma_code_obj = RomaCodeObj(code_obj);

    RomaString output;
    RomaString err_msg;
    auto result = helper.Process(roma_code_obj, output, err_msg);
    EXPECT_EQ(result, FailureExecutionResult(SC_ROMA_V8_WORKER_BAD_INPUT_ARGS));
  }

  // Unit test for empty input. JavaScript function can run with unmatched
  // arguments input, but cannot get valid output.
  {
    InvocationRequestSharedInput code_obj;
    auto input = vector<string>({"", "2"});
    GetExecutionObj(code_obj, input);
    auto roma_code_obj = RomaCodeObj(code_obj);

    RomaString output;
    RomaString err_msg;
    auto result = helper.Process(roma_code_obj, output, err_msg);
    EXPECT_EQ(result, SuccessExecutionResult());
    EXPECT_EQ(output, "null");
  }

  EXPECT_EQ(helper.Stop(), SuccessExecutionResult());
}

TEST_F(ExecutionManagerTest, UnSetIsolate) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
  ExecutionManager helper;

  InvocationRequestSharedInput ext_obj;
  auto input = vector<string>();
  GetExecutionObj(ext_obj, input);
  auto roma_code_obj = RomaCodeObj(ext_obj);

  RomaString output;
  RomaString err_msg;
  auto result = helper.Process(roma_code_obj, output, err_msg);
  EXPECT_EQ(result, FailureExecutionResult(
                        SC_ROMA_V8_WORKER_UNSET_ISOLATE_WITH_PRELOADED_CODE));
}

TEST_F(ExecutionManagerTest, UnmatchedCodeVersionNum) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);
  auto role_id = RoleId(0, false);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
  ExecutionManager helper;

  // Creates a blob.
  {
    CodeObject code_obj;
    // JS code has global variable which is updated by Handler.
    string js =
        "let carNum = 0;"
        "function Handler(a, b) { carNum = carNum + 1;"
        "return (a + b + carNum - 1); }";
    GetCodeObj(code_obj, js, 1);
    auto roma_code_obj = RomaCodeObj(code_obj);

    RomaString err_msg;
    vector<shared_ptr<FunctionBindingObjectBase>> function_bindings;

    intptr_t external_refs[] = {0};
    auto result =
        helper.Create(roma_code_obj, err_msg, function_bindings, external_refs);
    EXPECT_EQ(result, SuccessExecutionResult());
  }

  // Process the code.
  {
    for (int i = 0; i < 3; i++) {
      InvocationRequestSharedInput ext_obj;
      auto input = vector<string>({to_string(i), to_string(i)});
      GetExecutionObj(ext_obj, input, i);
      auto roma_code_obj = RomaCodeObj(ext_obj);

      RomaString output;
      RomaString err_msg;
      auto result = helper.Process(roma_code_obj, output, err_msg);
      if (i == 1) {
        EXPECT_EQ(result, SuccessExecutionResult());
        auto expected = to_string(i * 2);
        EXPECT_EQ(output, expected.c_str());
      } else {
        EXPECT_EQ(result, FailureExecutionResult(
                              SC_ROMA_V8_WORKER_UNMATCHED_CODE_VERSION_NUM));
      }
    }
  }

  EXPECT_EQ(helper.Stop(), SuccessExecutionResult());
}

}  // namespace google::scp::roma::worker::test
