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

#include "roma/sandbox/worker_api/sapi/src/worker_sandbox_api.h"

#include <gtest/gtest.h>

#include <signal.h>

#include <string>
#include <thread>
#include <vector>

#include "cc/roma/interface/function_binding_io.pb.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "roma/sandbox/constants/constants.h"
#include "roma/sandbox/worker_factory/src/worker_factory.h"

using google::scp::roma::proto::FunctionBindingIoProto;
using google::scp::roma::sandbox::constants::kCodeVersion;
using google::scp::roma::sandbox::constants::kHandlerName;
using google::scp::roma::sandbox::constants::kRequestAction;
using google::scp::roma::sandbox::constants::kRequestActionExecute;
using google::scp::roma::sandbox::constants::kRequestType;
using google::scp::roma::sandbox::constants::kRequestTypeJavascript;
using google::scp::roma::sandbox::worker::WorkerFactory;
using std::string;
using std::thread;
using std::vector;

namespace google::scp::roma::sandbox::worker_api::test {
TEST(WorkerSandboxApiTest, WorkerWorksThroughSandbox) {
  WorkerSandboxApi sandbox_api(
      WorkerFactory::WorkerEngine::v8, false /*require_preload*/,
      5 /*compilation_context_cache_size*/, -1 /*native_js_function_comms_fd*/,
      vector<string>() /*native_js_function_names*/, 0, 0, 0, 0);

  auto result = sandbox_api.Init();
  EXPECT_SUCCESS(result);

  result = sandbox_api.Run();
  EXPECT_SUCCESS(result);

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(
      "function cool_func() { return \"Hi there from sandboxed JS :)\" }");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  result = sandbox_api.RunCode(params_proto);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(params_proto.response(), "\"Hi there from sandboxed JS :)\"");

  result = sandbox_api.Stop();
  EXPECT_SUCCESS(result);
}

TEST(WorkerSandboxApiTest,
     StartingTheSandboxShouldFailIfNotEnoughMemoryInRlimitForV8) {
  // Since this is limiting the virtual memory space in a machine with swap and
  // no other limitations, this limit needs to be pretty high for V8 to properly
  // start. We set a limit of 100MB which causes a failure in this case.
  WorkerSandboxApi sandbox_api(
      WorkerFactory::WorkerEngine::v8, false /*require_preload*/,
      5 /*compilation_context_cache_size*/, -1 /*native_js_function_comms_fd*/,
      vector<string>() /*native_js_function_names*/,
      100 /*max_worker_virtual_memory_mb*/, 0, 0, 0);

  // Initializing the sandbox fail as we're giving a max of 100MB of virtual
  // space address for v8 and the sandbox.
  auto result = sandbox_api.Init();
  EXPECT_FALSE(result.Successful());

  result = sandbox_api.Stop();
  EXPECT_SUCCESS(result);
}

TEST(WorkerSandboxApiTest, WorkerCanCallHooksThroughSandbox) {
  int fds[2];
  EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds), 0);

  WorkerSandboxApi sandbox_api(
      WorkerFactory::WorkerEngine::v8, false /*require_preload*/,
      5 /*compilation_context_cache_size*/,
      fds[1] /*native_js_function_comms_fd*/, {"my_great_func"}, 0, 0, 0, 0);

  auto result = sandbox_api.Init();
  EXPECT_SUCCESS(result);

  thread to_handle_function_call(
      [](int fd) {
        sandbox2::Comms comms(fd);
        FunctionBindingIoProto io_proto;
        EXPECT_TRUE(comms.RecvProtoBuf(&io_proto));

        auto result = "from C++ " + io_proto.input_string();
        io_proto.set_output_string(result);

        EXPECT_TRUE(comms.SendProtoBuf(io_proto));
      },
      fds[0]);

  result = sandbox_api.Run();
  EXPECT_SUCCESS(result);

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(
      "function cool_func(input) { return my_great_func(input) };");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;
  params_proto.mutable_input()->Add("\"from JS\"");

  result = sandbox_api.RunCode(params_proto);

  to_handle_function_call.join();

  EXPECT_SUCCESS(result);
  EXPECT_EQ(params_proto.response(), "\"from C++ from JS\"");

  result = sandbox_api.Stop();
  EXPECT_SUCCESS(result);
}

class WorkerSandboxApiForTests : public WorkerSandboxApi {
 public:
  WorkerSandboxApiForTests(
      const worker::WorkerFactory::WorkerEngine& worker_engine,
      bool require_preload, int native_js_function_comms_fd,
      const std::vector<std::string>& native_js_function_names)
      : WorkerSandboxApi(worker_engine, require_preload, 5,
                         native_js_function_comms_fd, native_js_function_names,
                         0, 0, 0, 0) {}

  ::sapi::Sandbox* GetUnderlyingSandbox() { return worker_sapi_sandbox_.get(); }
};

TEST(WorkerSandboxApiTest, SandboxShouldComeBackUpIfItDies) {
  WorkerSandboxApiForTests sandbox_api(
      WorkerFactory::WorkerEngine::v8, false /*require_preload*/,
      -1 /*native_js_function_comms_fd*/,
      vector<string>() /*native_js_function_names*/);

  auto result = sandbox_api.Init();
  EXPECT_SUCCESS(result);

  result = sandbox_api.Run();
  EXPECT_SUCCESS(result);

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(
      "function cool_func() { return \"Hi there from sandboxed JS :)\" }");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  int sandbox_pid = sandbox_api.GetUnderlyingSandbox()->pid();
  EXPECT_EQ(0, kill(sandbox_pid, SIGKILL));
  // Wait for the sandbox to die
  while (sandbox_api.GetUnderlyingSandbox()->is_active()) {}

  result = sandbox_api.RunCode(params_proto);

  // We expect a failure since the worker process died
  EXPECT_FALSE(result.Successful());

  // Run code again and this time it should work
  result = sandbox_api.RunCode(params_proto);
  EXPECT_SUCCESS(result);
  EXPECT_EQ(params_proto.response(), "\"Hi there from sandboxed JS :)\"");

  result = sandbox_api.Stop();
  EXPECT_SUCCESS(result);
}

TEST(WorkerSandboxApiTest,
     SandboxShouldComeBackUpIfItDiesAndHooksShouldContinueWorking) {
  int fds[2];
  EXPECT_EQ(socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds), 0);

  WorkerSandboxApiForTests sandbox_api(
      WorkerFactory::WorkerEngine::v8, false /*require_preload*/,
      fds[1] /*native_js_function_comms_fd*/,
      {"my_great_func"} /*native_js_function_names*/);

  auto result = sandbox_api.Init();
  EXPECT_SUCCESS(result);

  thread to_handle_function_call(
      [](int fd) {
        sandbox2::Comms comms(fd);
        FunctionBindingIoProto io_proto;
        EXPECT_TRUE(comms.RecvProtoBuf(&io_proto));

        auto result = "from C++ hook :) " + io_proto.input_string();
        io_proto.set_output_string(result);

        EXPECT_TRUE(comms.SendProtoBuf(io_proto));
      },
      fds[0]);

  result = sandbox_api.Run();
  EXPECT_SUCCESS(result);

  ::worker_api::WorkerParamsProto params_proto;
  // Code calls a hook: "my_great_func"
  params_proto.set_code(
      "function cool_func(input) { return my_great_func(input) };");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;
  params_proto.mutable_input()->Add("\"from JS\"");

  int sandbox_pid = sandbox_api.GetUnderlyingSandbox()->pid();
  EXPECT_EQ(0, kill(sandbox_pid, SIGKILL));
  // Wait for the sandbox to die
  while (sandbox_api.GetUnderlyingSandbox()->is_active()) {}

  result = sandbox_api.RunCode(params_proto);
  // This is expected to fail since we killed the sandbox
  EXPECT_FALSE(result.Successful());

  // We run the code again and expect it to work this time around since the
  // sandbox should have been restarted
  result = sandbox_api.RunCode(params_proto);
  EXPECT_SUCCESS(result);

  to_handle_function_call.join();

  EXPECT_EQ(params_proto.response(), "\"from C++ hook :) from JS\"");

  result = sandbox_api.Stop();
  EXPECT_SUCCESS(result);
}
}  // namespace google::scp::roma::sandbox::worker_api::test
