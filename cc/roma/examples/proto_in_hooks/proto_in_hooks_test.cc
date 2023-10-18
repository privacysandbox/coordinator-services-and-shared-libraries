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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "core/test/utils/conditional_wait.h"
#include "roma/config/src/config.h"
// Include the proto generated code
#include "roma/examples/proto_in_hooks/proto/collection_of_doubles.pb.h"
#include "roma/interface/roma.h"

using absl::StatusOr;
using google::scp::core::test::WaitUntil;
using google::scp::roma::test::proto::CollectionOfDoublesProto;
using std::atomic;
using std::ifstream;
using std::make_unique;
using std::move;
using std::string;
using std::stringstream;
using std::unique_ptr;
using std::vector;

using namespace std::chrono_literals;  // NOLINT

namespace google::scp::roma::examples {
static string LoadFileAsString(string file_path) {
  ifstream file_stream(file_path);
  stringstream file_string_buffer;
  file_string_buffer << file_stream.rdbuf();
  return file_string_buffer.str();
}

void ProtoBytesInFunction(proto::FunctionBindingIoProto& io) {
  EXPECT_TRUE(io.has_input_bytes());
  auto byte_data = io.input_bytes().data();
  auto byte_len = io.input_bytes().size();

  CollectionOfDoublesProto proto;
  EXPECT_TRUE(proto.ParseFromArray(byte_data, byte_len));

  // Assert equality of the values that were sent from
  // examples/proto_in_hooks/js/proto_as_hook_argument.js
  EXPECT_EQ(3, proto.data().at(0).data().size());
  EXPECT_EQ(proto.data().at(0).data().at(0), 0.1);
  EXPECT_EQ(proto.data().at(0).data().at(1), 0.22);
  EXPECT_EQ(proto.data().at(0).data().at(2), 0.333);

  EXPECT_EQ(2, proto.data().at(1).data().size());
  EXPECT_EQ(proto.data().at(1).data().at(0), 0.9);
  EXPECT_EQ(proto.data().at(1).data().at(1), 0.1010);

  EXPECT_EQ(1, proto.metadata().size());
  EXPECT_EQ(proto.metadata().at("a key"), "a value");
}

TEST(ProtoInHooksTest, ShouldBeAbleToParseProtoBytesSentFromJS) {
  Config config;
  config.number_of_workers = 2;
  auto function_binding_object = make_unique<FunctionBindingObjectV2>();
  function_binding_object->function = ProtoBytesInFunction;
  // Register the function by the name it will be called from JS
  // This is called from examples/proto_in_hooks/js/proto_as_hook_argument.js
  function_binding_object->function_name = "send_proto_bytes_to_cpp";
  config.RegisterFunctionBinding(move(function_binding_object));

  auto status = RomaInit(config);
  EXPECT_TRUE(status.ok());

  auto js_source = LoadFileAsString(
      "cc/roma/examples/proto_in_hooks/js/proto_as_hook_argument_js.js");

  string result;
  atomic<bool> load_finished = false;
  atomic<bool> execute_finished = false;

  {
    auto code_obj = make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_num = 1;
    code_obj->js = js_source;

    status = LoadCodeObj(move(code_obj),
                         [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
                           EXPECT_TRUE(resp->ok());
                           load_finished.store(true);
                         });
    EXPECT_TRUE(status.ok());
  }
  WaitUntil([&]() { return load_finished.load(); }, 10s);

  {
    auto execution_obj = make_unique<InvocationRequestStrInput>();
    execution_obj->id = "foo";
    execution_obj->version_num = 1;
    // Handler name of exported function in JS source
    execution_obj->handler_name = "RomaHandlerSendBytes";

    status = Execute(move(execution_obj),
                     [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
                       EXPECT_TRUE(resp->ok());
                       auto& code_resp = **resp;
                       result = code_resp.resp;
                       execute_finished.store(true);
                     });
    EXPECT_TRUE(status.ok());
  }
  WaitUntil([&]() { return execute_finished.load(); }, 10s);

  // Assert the value returned from JS
  EXPECT_EQ(result, "\"Hello there from closure-compiled JS :)\"");

  status = RomaStop();
  EXPECT_TRUE(status.ok());
}

void ProtoBytesOutFunction(proto::FunctionBindingIoProto& io) {
  CollectionOfDoublesProto proto;
  // Add a metadata item (optional)
  (*proto.mutable_metadata())["some key"] = "some value";

  // Add two lists of double to the proto
  proto.add_data();
  proto.add_data();

  // Add two elements to the first list of doubles
  proto.mutable_data(0)->add_data(0.2);
  proto.mutable_data(0)->add_data(0.333);

  // Add three elements to the second list of doubles
  proto.mutable_data(1)->add_data(0.444);
  proto.mutable_data(1)->add_data(0.555);
  proto.mutable_data(1)->add_data(0.666);

  int serialized_size = proto.ByteSizeLong();
  vector<char> serialized_data(serialized_size);
  EXPECT_TRUE(proto.SerializeToArray(serialized_data.data(), serialized_size));

  // Set the output bytes
  io.set_output_bytes(serialized_data.data(), serialized_size);
}

TEST(ProtoInHooksTest, ShouldBeAbleToWriteProtoBinaryDataToJS) {
  Config config;
  config.number_of_workers = 2;
  auto function_binding_object = make_unique<FunctionBindingObjectV2>();
  function_binding_object->function = ProtoBytesOutFunction;
  // Register the function by the name it will be called from JS
  // This is called from examples/proto_in_hooks/js/proto_as_hook_argument.js
  function_binding_object->function_name = "get_proto_bytes_from_cpp";
  config.RegisterFunctionBinding(move(function_binding_object));

  auto status = RomaInit(config);
  EXPECT_TRUE(status.ok());

  auto js_source = LoadFileAsString(
      "cc/roma/examples/proto_in_hooks/js/proto_as_hook_argument_js.js");

  string result;
  atomic<bool> load_finished = false;
  atomic<bool> execute_finished = false;

  {
    auto code_obj = make_unique<CodeObject>();
    code_obj->id = "foo";
    code_obj->version_num = 1;
    code_obj->js = js_source;

    status = LoadCodeObj(move(code_obj),
                         [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
                           EXPECT_TRUE(resp->ok());
                           load_finished.store(true);
                         });
    EXPECT_TRUE(status.ok());
  }
  WaitUntil([&]() { return load_finished.load(); }, 10s);

  {
    auto execution_obj = make_unique<InvocationRequestStrInput>();
    execution_obj->id = "foo";
    execution_obj->version_num = 1;
    // Handler name of exported function in JS source
    execution_obj->handler_name = "RomaHandlerGetBytes";

    status = Execute(move(execution_obj),
                     [&](unique_ptr<absl::StatusOr<ResponseObject>> resp) {
                       EXPECT_TRUE(resp->ok());
                       auto& code_resp = **resp;
                       result = code_resp.resp;
                       execute_finished.store(true);
                     });
    EXPECT_TRUE(status.ok());
  }
  WaitUntil([&]() { return execute_finished.load(); }, 10s);

  // Assert the value returned from JS. The JS code deserializes the proto bytes
  // back into a proto object, then turns it into a JS object and returns it. So
  // here we get JSON.stringify'd version of that object. This is just for the
  // purpose of asserting that the data was correctly deserialized in JS.
  EXPECT_EQ(
      result,
      "{\"m\":[[\"some key\",\"some "
      "value\"]],\"j\":[{\"j\":[0.2,0.333]},{\"j\":[0.444,0.555,0.666]}]}");

  status = RomaStop();
  EXPECT_TRUE(status.ok());
}
}  // namespace google::scp::roma::examples
