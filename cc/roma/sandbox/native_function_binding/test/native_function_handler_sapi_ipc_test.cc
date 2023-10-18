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

#include "roma/sandbox/native_function_binding/src/native_function_handler_sapi_ipc.h"

#include <gtest/gtest.h>

#include <sys/socket.h>

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/test/utils/auto_init_run_stop.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "roma/sandbox/constants/constants.h"
#include "roma/sandbox/native_function_binding/src/native_function_table.h"
#include "sandboxed_api/sandbox2/comms.h"

using google::scp::core::test::AutoInitRunStop;
using google::scp::roma::sandbox::constants::
    kFuctionBindingMetadataFunctionName;
using google::scp::roma::sandbox::native_function_binding::
    NativeFunctionHandlerSapiIpc;
using google::scp::roma::sandbox::native_function_binding::NativeFunctionTable;
using std::make_shared;
using std::string;
using std::vector;

namespace google::scp::roma::sandbox::native_function_binding::test {
TEST(NativeFunctionHandlerSapiIpcTest, IninRunStop) {
  int fd_pair[2];
  EXPECT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_pair));
  vector<int> local_fds = {fd_pair[0]};
  vector<int> remote_fds = {fd_pair[1]};
  auto function_table = make_shared<NativeFunctionTable>();
  NativeFunctionHandlerSapiIpc handler(function_table, local_fds, remote_fds);

  EXPECT_SUCCESS(handler.Init());
  EXPECT_SUCCESS(handler.Run());
  EXPECT_SUCCESS(handler.Stop());
}

static bool g_called_registered_function;

void FunctionToBeCalled(proto::FunctionBindingIoProto& io_proto) {
  g_called_registered_function = true;
  io_proto.set_output_string("I'm an output standalone string");
}

TEST(NativeFunctionHandlerSapiIpcTest, ShouldCallFunctionWhenRegistered) {
  int fd_pair[2];
  EXPECT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_pair));
  vector<int> local_fds = {fd_pair[0]};
  vector<int> remote_fds = {fd_pair[1]};
  auto function_table = make_shared<NativeFunctionTable>();
  function_table->Register("cool_function_name", FunctionToBeCalled);
  NativeFunctionHandlerSapiIpc handler(function_table, local_fds, remote_fds);
  AutoInitRunStop for_handler(handler);

  g_called_registered_function = false;

  auto remote_fd = remote_fds.at(0);
  sandbox2::Comms comms(remote_fd);
  proto::FunctionBindingIoProto io_proto;
  (*io_proto.mutable_metadata())[kFuctionBindingMetadataFunctionName] =
      "cool_function_name";
  // Send the request over so that it's handled and the registered function
  // can be called
  EXPECT_TRUE(comms.SendProtoBuf(io_proto));
  // Receive the response
  EXPECT_TRUE(comms.RecvProtoBuf(&io_proto));

  EXPECT_TRUE(g_called_registered_function);
  EXPECT_EQ("I'm an output standalone string", io_proto.output_string());
}

TEST(NativeFunctionHandlerSapiIpcTest,
     ShouldAddErrorsIfFunctionNameIsNotFoundInTable) {
  int fd_pair[2];
  EXPECT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_pair));
  vector<int> local_fds = {fd_pair[0]};
  vector<int> remote_fds = {fd_pair[1]};
  auto function_table = make_shared<NativeFunctionTable>();
  // We don't register any functions with the function table
  NativeFunctionHandlerSapiIpc handler(function_table, local_fds, remote_fds);
  AutoInitRunStop for_handler(handler);

  g_called_registered_function = false;

  auto remote_fd = remote_fds.at(0);
  sandbox2::Comms comms(remote_fd);
  proto::FunctionBindingIoProto io_proto;
  (*io_proto.mutable_metadata())[kFuctionBindingMetadataFunctionName] =
      "cool_function_name";
  // Send the request over so that it's handled and the registered function
  // can be called
  EXPECT_TRUE(comms.SendProtoBuf(io_proto));
  // Receive the response
  EXPECT_TRUE(comms.RecvProtoBuf(&io_proto));

  EXPECT_FALSE(g_called_registered_function);
  EXPECT_FALSE(io_proto.has_input_string() ||
               io_proto.has_input_list_of_string() ||
               io_proto.has_input_map_of_string());
  EXPECT_GE(io_proto.errors().size(), 0);
  EXPECT_EQ(io_proto.errors(0), "ROMA: Failed to execute the C++ function.");
}

TEST(NativeFunctionHandlerSapiIpcTest,
     ShouldAddErrorsIfFunctionNameIsNotInMetadata) {
  int fd_pair[2];
  EXPECT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_pair));
  vector<int> local_fds = {fd_pair[0]};
  vector<int> remote_fds = {fd_pair[1]};
  auto function_table = make_shared<NativeFunctionTable>();
  // We don't register any functions with the function table
  NativeFunctionHandlerSapiIpc handler(function_table, local_fds, remote_fds);
  AutoInitRunStop for_handler(handler);

  g_called_registered_function = false;

  auto remote_fd = remote_fds.at(0);
  sandbox2::Comms comms(remote_fd);
  proto::FunctionBindingIoProto io_proto;
  // Send the request over so that it's handled and the registered function
  // can be called
  EXPECT_TRUE(comms.SendProtoBuf(io_proto));
  // Receive the response
  EXPECT_TRUE(comms.RecvProtoBuf(&io_proto));

  EXPECT_FALSE(g_called_registered_function);
  EXPECT_FALSE(io_proto.has_input_string() ||
               io_proto.has_input_list_of_string() ||
               io_proto.has_input_map_of_string());
  EXPECT_GE(io_proto.errors().size(), 0);
  EXPECT_EQ(io_proto.errors(0), "ROMA: Could not find C++ function by name.");
}

static bool g_called_registered_function_one;
static bool g_called_registered_function_two;

void FunctionOne(proto::FunctionBindingIoProto& io_proto) {
  g_called_registered_function_one = true;
  io_proto.set_output_string("From function one");
}

void FunctionTwo(proto::FunctionBindingIoProto& io_proto) {
  g_called_registered_function_two = true;
  io_proto.set_output_string("From function two");
}

TEST(NativeFunctionHandlerSapiIpcTest, ShouldBeAbleToCallMultipleFunctions) {
  int fd_pair[2];
  EXPECT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_pair));
  vector<int> local_fds = {fd_pair[0]};
  vector<int> remote_fds = {fd_pair[1]};
  auto function_table = make_shared<NativeFunctionTable>();
  function_table->Register("cool_function_name_one", FunctionOne);
  function_table->Register("cool_function_name_two", FunctionTwo);
  NativeFunctionHandlerSapiIpc handler(function_table, local_fds, remote_fds);
  AutoInitRunStop for_handler(handler);

  g_called_registered_function_one = false;
  g_called_registered_function_two = false;

  auto remote_fd = remote_fds.at(0);
  sandbox2::Comms comms(remote_fd);
  proto::FunctionBindingIoProto io_proto;
  (*io_proto.mutable_metadata())[kFuctionBindingMetadataFunctionName] =
      "cool_function_name_one";
  // Send the request over so that it's handled and the registered function
  // can be called
  EXPECT_TRUE(comms.SendProtoBuf(io_proto));
  // Receive the response
  EXPECT_TRUE(comms.RecvProtoBuf(&io_proto));

  EXPECT_TRUE(g_called_registered_function_one);
  EXPECT_EQ(io_proto.errors().size(), 0);
  EXPECT_EQ("From function one", io_proto.output_string());

  io_proto.Clear();
  (*io_proto.mutable_metadata())[kFuctionBindingMetadataFunctionName] =
      "cool_function_name_two";
  // Send the request over so that it's handled and the registered function
  // can be called
  EXPECT_TRUE(comms.SendProtoBuf(io_proto));
  // Receive the response
  EXPECT_TRUE(comms.RecvProtoBuf(&io_proto));

  EXPECT_TRUE(g_called_registered_function_two);
  EXPECT_EQ(io_proto.errors().size(), 0);
  EXPECT_EQ("From function two", io_proto.output_string());
}
}  // namespace google::scp::roma::sandbox::native_function_binding::test
