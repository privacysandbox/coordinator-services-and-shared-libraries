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

#include "roma/worker/src/worker.h"

#include <gtest/gtest.h>

#include <v8-context.h>
#include <v8-initialization.h>
#include <v8-isolate.h>
#include <v8-local-handle.h>
#include <v8-primitive.h>
#include <v8-script.h>

#include <linux/limits.h>

#include <atomic>
#include <memory>
#include <vector>

#include "core/test/utils/auto_init_run_stop.h"
#include "core/test/utils/conditional_wait.h"
#include "include/libplatform/libplatform.h"
#include "roma/common/src/process.h"
#include "roma/ipc/src/ipc_manager.h"
#include "roma/worker/src/error_codes.h"

using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::SC_ROMA_V8_WORKER_SCRIPT_EXECUTION_TIMEOUT;
using google::scp::core::errors::SC_ROMA_V8_WORKER_UNMATCHED_CODE_VERSION_NUM;
using google::scp::core::test::AutoInitRunStop;
using google::scp::core::test::WaitUntil;
using google::scp::roma::Callback;
using google::scp::roma::common::Process;
using google::scp::roma::common::RoleId;
using google::scp::roma::ipc::IpcManager;
using google::scp::roma::ipc::Request;
using google::scp::roma::ipc::RequestType;
using google::scp::roma::ipc::Response;
using google::scp::roma::worker::Worker;
using std::atomic;
using std::make_shared;
using std::make_unique;
using std::shared_ptr;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;

namespace google::scp::roma::worker::test {
static constexpr char kTimeoutMsTag[] = "TimeoutMs";

class WorkerTest : public ::testing::Test {
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

  static unique_ptr<v8::Platform> platform_;
};

unique_ptr<v8::Platform> WorkerTest::platform_{nullptr};

/**
 * @brief v8_worker has different execute flow for JS code, wasm code and JS
 * mixed with wasm code. For JS code, the context cached with snapshot blob.
 * While snapshot blob doesn't work for wasm code, the temp solution is
 * Compile&Run wasm code and JS mixed with wasm code when process the request.
 * This unit test is testing v8_worker can process and switch between different
 * type of code.
 */
TEST_F(WorkerTest, ExecuteJsOrWasmOrJsMixedWithWasmCode) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);

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
  char wasm_bin[] = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01,
                     0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03,
                     0x02, 0x01, 0x00, 0x07, 0x07, 0x01, 0x03, 0x61, 0x64,
                     0x64, 0x00, 0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20,
                     0x00, 0x20, 0x01, 0x6a, 0x0b};

  auto role_id = RoleId(0, false);
  auto& ipc = IpcManager::Instance()->GetIpcChannel(role_id);
  {
    IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
    Callback callback;

    // 1st version source code is pure JS code. The first request is code update
    // request and other is execution request.
    for (auto i = 0; i < 5; i++) {
      unique_ptr<Request> request;
      if (i == 0) {
        CodeObject obj;
        obj.id = to_string(i);
        obj.version_num = 1;
        obj.js =
            "let increase = 0;"
            "function Handler(a, b) { increase += 1; "
            "var match = a + b+ increase -1; return match; }";
        request = make_unique<Request>(make_unique<CodeObject>(obj), callback,
                                       RequestType::kUpdate);

      } else {
        InvocationRequestSharedInput obj;
        obj.id = to_string(i);
        obj.version_num = 1;
        obj.handler_name = "Handler";
        obj.input = vector<shared_ptr<string>>(
            {make_shared<string>(to_string(i).c_str()),
             make_shared<string>(to_string(i).c_str())});
        request =
            make_unique<Request>(make_unique<InvocationRequestSharedInput>(obj),
                                 callback, RequestType::kExecute);
      }
      EXPECT_TRUE(ipc.TryAcquirePushRequest().Successful());
      ipc.PushRequest(move(request));
    }

    // 2nd version source code is JS mixed with global webssambly code. The
    // first request is code update request and other is execution request.
    for (auto i = 5; i < 10; i++) {
      unique_ptr<Request> request;
      if (i == 5) {
        CodeObject obj;
        obj.id = to_string(i);
        obj.version_num = 2;
        obj.js = js;
        request = make_unique<Request>(make_unique<CodeObject>(obj), callback,
                                       RequestType::kUpdate);
      } else {
        InvocationRequestSharedInput obj;
        obj.id = to_string(i);
        obj.version_num = 2;
        obj.handler_name = "Handler";
        obj.input = vector<shared_ptr<string>>(
            {make_shared<string>(to_string(i).c_str()),
             make_shared<string>(to_string(i).c_str())});
        request =
            make_unique<Request>(make_unique<InvocationRequestSharedInput>(obj),
                                 callback, RequestType::kExecute);
      }
      EXPECT_TRUE(ipc.TryAcquirePushRequest().Successful());
      ipc.PushRequest(move(request));
    }

    // 3rd version source code is pure wasm code. The first request is code
    // update request and other is execution request.
    for (auto i = 10; i < 15; i++) {
      unique_ptr<Request> request;
      if (i == 10) {
        CodeObject obj;
        obj.id = to_string(i);
        obj.version_num = 3;
        obj.wasm.assign(wasm_bin, sizeof(wasm_bin));
        request = make_unique<Request>(make_unique<CodeObject>(obj), callback,
                                       RequestType::kUpdate);
      } else {
        InvocationRequestSharedInput obj;
        obj.id = to_string(i);
        obj.version_num = 3;
        obj.wasm_return_type = WasmDataType::kUint32;
        obj.handler_name = "add";
        obj.input = vector<shared_ptr<string>>(
            {make_shared<string>(to_string(i).c_str()),
             make_shared<string>(to_string(i).c_str())});
        request =
            make_unique<Request>(make_unique<InvocationRequestSharedInput>(obj),
                                 callback, RequestType::kExecute);
      }
      EXPECT_TRUE(ipc.TryAcquirePushRequest().Successful());
      ipc.PushRequest(move(request));
    }
  }

  Worker v8_worker(role_id);
  EXPECT_EQ(v8_worker.Init(), SuccessExecutionResult());

  auto func = [&]() {
    IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
    return v8_worker.Run();
  };
  pid_t child_pid;
  Process::Create(func, child_pid);

  unique_ptr<Response> response;
  for (auto i = 0; i < 15; i++) {
    EXPECT_EQ(ipc.PopResponse(response), SuccessExecutionResult());
    EXPECT_EQ(response->result, SuccessExecutionResult());
    EXPECT_EQ(string(response->response->id), to_string(i));

    // Only check the responses for execution requests. When i equals to 0/5/10,
    // these requests are code object loading request. There is no response.
    if (i != 0 && i != 5 && i != 10) {
      EXPECT_EQ(string(response->response->resp), to_string(i * 2));
    }
  }

  EXPECT_EQ(v8_worker.Stop(), SuccessExecutionResult());
  ipc.ReleaseLocks();
  int child_exit_status;
  waitpid(child_pid, &child_exit_status, 0);
}

/**
 * @brief v8_worker uses a snapshot blob to store the context with compiled
 * JS code. Here, the JS code object has a global variable which updated by the
 * Handler function, and this unit test verify that v8_worker will create a
 * clear context for each code object execution.
 */
TEST_F(WorkerTest, CleanCompiledDefaultContext) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);

  auto role_id = RoleId(0, false);
  auto& ipc = IpcManager::Instance()->GetIpcChannel(role_id);
  {
    IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
    Callback callback;

    for (auto i = 0; i < 3; i++) {
      unique_ptr<Request> request;
      if (i == 0) {
        CodeObject obj;
        obj.id = to_string(i);
        obj.version_num = 1;
        obj.js =
            "let increase = 0;"
            "function Handler(input) { increase += 1; "
            "var match = input + increase -1; return match; }";

        request = make_unique<Request>(make_unique<CodeObject>(obj), callback,
                                       RequestType::kUpdate);
      } else {
        InvocationRequestSharedInput ext_obj;
        ext_obj.id = to_string(i);
        ext_obj.version_num = 1;
        ext_obj.handler_name = "Handler";
        ext_obj.input.push_back(make_shared<string>(to_string(i).c_str()));

        request = make_unique<Request>(
            make_unique<InvocationRequestSharedInput>(ext_obj), callback,
            RequestType::kExecute);
      }
      EXPECT_TRUE(ipc.TryAcquirePushRequest().Successful());
      ipc.PushRequest(move(request));
    }

    for (auto i = 3; i < 6; i++) {
      unique_ptr<Request> request;
      if (i == 3) {
        CodeObject obj;
        obj.id = to_string(i);
        obj.version_num = 2;
        obj.js =
            "let increase = 1;"
            "function Handler(input) { increase += 1; "
            "var match = input + increase -1; return match; }";

        request = make_unique<Request>(make_unique<CodeObject>(obj), callback,
                                       RequestType::kUpdate);
      } else {
        InvocationRequestSharedInput ext_obj;
        ext_obj.id = to_string(i);
        ext_obj.version_num = 2;
        ext_obj.handler_name = "Handler";
        ext_obj.input.push_back(make_shared<string>(to_string(i).c_str()));

        request = make_unique<Request>(
            make_unique<InvocationRequestSharedInput>(ext_obj), callback,
            RequestType::kExecute);
      }
      EXPECT_TRUE(ipc.TryAcquirePushRequest().Successful());
      ipc.PushRequest(move(request));
    }
  }

  Worker v8_worker(role_id);
  EXPECT_EQ(v8_worker.Init(), SuccessExecutionResult());

  auto func = [&]() {
    IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
    return v8_worker.Run();
  };
  pid_t child_pid;
  Process::Create(func, child_pid);

  unique_ptr<Response> response;
  for (auto i = 0; i < 6; i++) {
    EXPECT_EQ(ipc.PopResponse(response), SuccessExecutionResult());
    EXPECT_EQ(response->result, SuccessExecutionResult())
        << GetErrorMessage(response->result.status_code);
    EXPECT_EQ(string(response->response->id), to_string(i));

    // Only check the responses for execution requests. When i equals to 0 or 3,
    // these requests are code object loading request. There is no response.
    if (i != 0 && i != 3) {
      if (i < 3) {
        EXPECT_EQ(string(response->response->resp), to_string(i));
      } else {
        EXPECT_EQ(string(response->response->resp), to_string(i + 1));
      }
    }
  }

  EXPECT_EQ(v8_worker.Stop(), SuccessExecutionResult());
  ipc.ReleaseLocks();
  int child_exit_status;
  waitpid(child_pid, &child_exit_status, 0);
}

/**
 * @brief In v8_worker, only code execution step is being watched by
 * execution_manager for timeout. In this unit test, JS code has true
 * infinite loop. The v8 won't be infinite in the step snapshot blob creation,
 * and timeout in code execution step works.
 */
TEST_F(WorkerTest, TimeoutTrueInfiniteLoop) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);

  auto role_id = RoleId(0, false);
  auto& ipc = IpcManager::Instance()->GetIpcChannel(role_id);
  IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
  {
    CodeObject obj;
    obj.id = "id";
    obj.version_num = 1;
    obj.js =
        "function Handler() { while (true) {};"
        "return a;}";
    obj.tags[kTimeoutMsTag] = "100";
    Callback callback;
    auto request = make_unique<Request>(make_unique<CodeObject>(obj), callback,
                                        RequestType::kUpdate);
    ipc.PushRequest(move(request));
  }

  {
    InvocationRequestSharedInput obj;
    obj.id = "id";
    obj.version_num = 1;
    obj.handler_name = "Handler";
    obj.tags[kTimeoutMsTag] = "100";
    Callback callback;
    auto request =
        make_unique<Request>(make_unique<InvocationRequestSharedInput>(obj),
                             callback, RequestType::kExecute);
    ipc.PushRequest(move(request));
  }

  Worker v8_worker(role_id);
  EXPECT_EQ(v8_worker.Init(), SuccessExecutionResult());

  auto func = [&]() {
    IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
    return v8_worker.Run();
  };
  pid_t child_pid;
  Process::Create(func, child_pid);

  unique_ptr<Response> response;

  while (ipc.PopResponse(response) != SuccessExecutionResult()) {}
  EXPECT_EQ(response->result, SuccessExecutionResult());

  // Pop out the response for code update request.
  while (ipc.PopResponse(response) != SuccessExecutionResult()) {}
  EXPECT_EQ(response->result,
            FailureExecutionResult(SC_ROMA_V8_WORKER_SCRIPT_EXECUTION_TIMEOUT));
  EXPECT_EQ(string(response->response->resp), "");

  EXPECT_EQ(v8_worker.Stop(), SuccessExecutionResult());
  manager->ReleaseLocks();
  int child_exit_status;
  waitpid(child_pid, &child_exit_status, 0);
}

/**
 * @brief In this unit test, two requests are executed. The first has infinite
 * while loop, the second loop has normal execution time. Worker can terminate
 * the first request with timeout error and continue execute the second
 * request.
 */
TEST_F(WorkerTest, DefaultExecutionTimeout) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);

  auto role_id = RoleId(0, false);
  auto& ipc = IpcManager::Instance()->GetIpcChannel(role_id);
  {
    IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
    Callback callback;
    {
      CodeObject obj;
      obj.id = "id";
      obj.version_num = 1;
      obj.js =
          "function Handler(a) { while (a == 0) {};"
          "return a;}";
      auto request =
          make_unique<Request>(make_unique<CodeObject>(obj), callback);
      EXPECT_TRUE(ipc.TryAcquirePushRequest().Successful());
      ipc.PushRequest(move(request));
    }

    for (auto i = 0; i < 4; i++) {
      InvocationRequestSharedInput obj;
      obj.id = "id";
      obj.version_num = 1;
      obj.handler_name = "Handler";
      obj.input.push_back(make_shared<string>(to_string(i)));
      auto request = make_unique<Request>(
          make_unique<InvocationRequestSharedInput>(obj), callback);
      EXPECT_TRUE(ipc.TryAcquirePushRequest().Successful());
      ipc.PushRequest(move(request));
    }
  }

  Worker v8_worker(role_id);
  EXPECT_EQ(v8_worker.Init(), SuccessExecutionResult());

  auto func = [&]() {
    IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
    return v8_worker.Run();
  };
  pid_t child_pid;
  Process::Create(func, child_pid);
  unique_ptr<Response> response;

  // Pop out the response for code update request.
  while (!ipc.PopResponse(response).Successful()) {}
  EXPECT_EQ(response->result, SuccessExecutionResult());

  for (auto i = 0; i < 2; i++) {
    while (!ipc.PopResponse(response).Successful()) {}
    if (i == 0) {
      EXPECT_EQ(
          response->result,
          FailureExecutionResult(SC_ROMA_V8_WORKER_SCRIPT_EXECUTION_TIMEOUT));
      EXPECT_EQ(string(response->response->resp), "");
    } else {
      EXPECT_EQ(response->result, SuccessExecutionResult());
      EXPECT_EQ(string(response->response->id), "id");
      EXPECT_EQ(string(response->response->resp), to_string(i));
    }
  }

  EXPECT_EQ(v8_worker.Stop(), SuccessExecutionResult());
  manager->ReleaseLocks();
  int child_exit_status;
  waitpid(child_pid, &child_exit_status, 0);
}

TEST_F(WorkerTest, CustomizedExecuteTimeout) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);

  auto role_id = RoleId(0, false);
  auto& ipc = IpcManager::Instance()->GetIpcChannel(role_id);
  {
    IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

    Callback callback;
    {
      CodeObject obj;
      obj.id = "id";
      obj.version_num = 1;
      obj.js =
          "function sleep(milliseconds) {"
          "const date = Date.now();"
          "let currentDate = null;"
          "do {"
          "currentDate = Date.now();"
          "} while (currentDate - date < milliseconds);"
          "}"
          "function Handler(a) {"
          "sleep(200);"
          "return a;}";
      auto request =
          make_unique<Request>(make_unique<CodeObject>(obj), callback);
      EXPECT_TRUE(ipc.TryAcquirePushRequest().Successful());
      ipc.PushRequest(move(request));
    }

    for (auto i = 0; i < 5; i++) {
      InvocationRequestSharedInput obj;
      obj.id = "id";
      obj.version_num = 1;
      obj.handler_name = "Handler";
      obj.input.push_back(make_shared<string>(to_string(i)));
      if (i < 3) {
        // Timeout threshold value is smaller than the code execution time (200
        // ms). This request should fail.
        obj.tags[kTimeoutMsTag] = "100";
      } else {
        // Timeout threshold value is larger than the code execution time (200
        // ms). This request should success.
        obj.tags[kTimeoutMsTag] = "300";
      }
      auto request = make_unique<Request>(
          make_unique<InvocationRequestSharedInput>(obj), callback);
      EXPECT_TRUE(ipc.TryAcquirePushRequest().Successful());
      ipc.PushRequest(move(request));
    }
  }

  Worker v8_worker(role_id);
  EXPECT_EQ(v8_worker.Init(), SuccessExecutionResult());

  auto func = [&]() {
    IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
    return v8_worker.Run();
  };
  pid_t child_pid;
  Process::Create(func, child_pid);

  unique_ptr<Response> response;

  // Pop out the response for code update request.
  while (!ipc.PopResponse(response).Successful()) {}
  EXPECT_EQ(response->result, SuccessExecutionResult());

  for (auto i = 0; i < 5; i++) {
    while (!ipc.PopResponse(response).Successful()) {}
    // For i < 3, the request timeout threshold value is smaeller than code
    // execution time. Check whether the requests are failed.
    if (i < 3) {
      EXPECT_EQ(
          response->result,
          FailureExecutionResult(SC_ROMA_V8_WORKER_SCRIPT_EXECUTION_TIMEOUT));
      EXPECT_EQ(string(response->response->resp), "");
    } else {
      EXPECT_EQ(response->result, SuccessExecutionResult());
      EXPECT_EQ(string(response->response->id), "id");
      EXPECT_EQ(string(response->response->resp), to_string(i));
    }
  }

  EXPECT_EQ(v8_worker.Stop(), SuccessExecutionResult());
  manager->ReleaseLocks();
  int child_exit_status;
  waitpid(child_pid, &child_exit_status, 0);
}

TEST_F(WorkerTest, FailedWithUnmatchVersionNum) {
  unique_ptr<IpcManager> manager(IpcManager::Create(1));
  AutoInitRunStop auto_init_run_stop(*manager);

  auto role_id = RoleId(0, false);
  auto& ipc = IpcManager::Instance()->GetIpcChannel(role_id);
  {
    IpcManager::Instance()->SetUpIpcForMyProcess(role_id);

    Callback callback;
    {
      CodeObject obj;
      obj.id = "id";
      obj.version_num = 1;
      obj.js =
          "let increase = 0;"
          "function Handler(a, b) { increase += 1; "
          "var match = a + b+ increase -1; return match; }";
      auto request =
          make_unique<Request>(make_unique<CodeObject>(obj), callback);
      EXPECT_TRUE(ipc.TryAcquirePushRequest().Successful());
      ipc.PushRequest(move(request));
    }

    for (auto i = 0; i < 3; i++) {
      InvocationRequestSharedInput obj;
      obj.id = "id";
      obj.version_num = i;
      obj.handler_name = "Handler";
      obj.input = vector<shared_ptr<string>>(
          {make_shared<string>(to_string(i).c_str()),
           make_shared<string>(to_string(i).c_str())});
      auto request = make_unique<Request>(
          make_unique<InvocationRequestSharedInput>(obj), callback);
      EXPECT_TRUE(ipc.TryAcquirePushRequest().Successful());
      ipc.PushRequest(move(request));
    }
  }

  Worker v8_worker(role_id);
  EXPECT_EQ(v8_worker.Init(), SuccessExecutionResult());

  auto func = [&]() {
    IpcManager::Instance()->SetUpIpcForMyProcess(role_id);
    return v8_worker.Run();
  };
  pid_t child_pid;
  Process::Create(func, child_pid);

  unique_ptr<Response> response;

  // Pop out the response for code update request.
  while (!ipc.PopResponse(response).Successful()) {}
  EXPECT_EQ(response->result, SuccessExecutionResult());

  for (auto i = 0; i < 3; i++) {
    while (!ipc.PopResponse(response).Successful()) {}

    // The pre-loaded code object version is 1. All execution request with
    // different version num will fail.
    if (i != 1) {
      EXPECT_EQ(
          response->result,
          FailureExecutionResult(SC_ROMA_V8_WORKER_UNMATCHED_CODE_VERSION_NUM));
      EXPECT_EQ(string(response->response->resp), "");
    } else {
      EXPECT_EQ(response->result, SuccessExecutionResult());
      EXPECT_EQ(string(response->response->id), "id");
      EXPECT_EQ(string(response->response->resp), to_string(i * 2));
    }
  }

  EXPECT_EQ(v8_worker.Stop(), SuccessExecutionResult());
  manager->ReleaseLocks();
  int child_exit_status;
  waitpid(child_pid, &child_exit_status, 0);
}
}  // namespace google::scp::roma::worker::test
