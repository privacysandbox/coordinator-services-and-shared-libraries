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

#include "roma/sandbox/native_function_binding/src/native_function_invoker_sapi_ipc.h"

#include <gtest/gtest.h>

#include <sys/socket.h>

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/test/utils/auto_init_run_stop.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "roma/sandbox/constants/constants.h"
#include "sandboxed_api/sandbox2/comms.h"

using google::scp::core::test::AutoInitRunStop;
using google::scp::roma::sandbox::constants::
    kFuctionBindingMetadataFunctionName;
using google::scp::roma::sandbox::native_function_binding::
    NativeFunctionInvokerSapiIpc;
using std::make_shared;
using std::string;
using std::thread;
using std::vector;

namespace google::scp::roma::sandbox::native_function_binding::test {
TEST(NativeFunctionHandlerSapiIpcTest, ShouldReturnFailureOnInvokeIfBadFd) {
  NativeFunctionInvokerSapiIpc invoker(-1);

  proto::FunctionBindingIoProto io_proto;
  EXPECT_FALSE(invoker.Invoke("func_name", io_proto).Successful());
}

TEST(NativeFunctionHandlerSapiIpcTest, ShouldMakeCallOnFd) {
  int fd_pair[2];
  EXPECT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_pair));

  NativeFunctionInvokerSapiIpc invoker(fd_pair[0]);

  thread to_handle_message([fd = fd_pair[1]]() {
    sandbox2::Comms comms(fd);
    proto::FunctionBindingIoProto io_proto;
    EXPECT_TRUE(comms.RecvProtoBuf(&io_proto));
    io_proto.set_output_string("Some string");
    EXPECT_TRUE(comms.SendProtoBuf(io_proto));
  });

  proto::FunctionBindingIoProto io_proto;
  EXPECT_SUCCESS(invoker.Invoke("func_name", io_proto));

  to_handle_message.join();

  EXPECT_EQ("Some string", io_proto.output_string());
}

TEST(NativeFunctionHandlerSapiIpcTest, ShouldAppendFunctionNameToMetadata) {
  int fd_pair[2];
  EXPECT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_pair));

  NativeFunctionInvokerSapiIpc invoker(fd_pair[0]);

  thread to_handle_message([fd = fd_pair[1]]() {
    sandbox2::Comms comms(fd);
    proto::FunctionBindingIoProto io_proto;
    EXPECT_TRUE(comms.RecvProtoBuf(&io_proto));
    EXPECT_EQ("func_name",
              io_proto.metadata().at(kFuctionBindingMetadataFunctionName));
    io_proto.set_output_string("Some string");
    EXPECT_TRUE(comms.SendProtoBuf(io_proto));
  });

  proto::FunctionBindingIoProto io_proto;
  EXPECT_SUCCESS(invoker.Invoke("func_name", io_proto));

  to_handle_message.join();

  EXPECT_EQ("Some string", io_proto.output_string());
}
}  // namespace google::scp::roma::sandbox::native_function_binding::test
