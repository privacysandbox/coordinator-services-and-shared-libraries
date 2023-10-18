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

#include "roma/sandbox/worker_api/sapi/src/worker_wrapper.h"

#include <gtest/gtest.h>

#include "public/core/test/interface/execution_result_matchers.h"
#include "roma/sandbox/constants/constants.h"
#include "roma/sandbox/worker_api/sapi/src/worker_init_params.pb.h"
#include "roma/sandbox/worker_factory/src/worker_factory.h"
#include "sandboxed_api/lenval_core.h"

using google::scp::roma::sandbox::constants::kCodeVersion;
using google::scp::roma::sandbox::constants::kHandlerName;
using google::scp::roma::sandbox::constants::kRequestAction;
using google::scp::roma::sandbox::constants::kRequestActionExecute;
using google::scp::roma::sandbox::constants::kRequestType;
using google::scp::roma::sandbox::constants::kRequestTypeJavascript;
using google::scp::roma::sandbox::worker::WorkerFactory;

namespace google::scp::roma::sandbox::worker_api::test {
static ::worker_api::WorkerInitParamsProto GetDefaultInitParams() {
  ::worker_api::WorkerInitParamsProto init_params;
  init_params.set_worker_factory_js_engine(
      static_cast<int>(worker::WorkerFactory::WorkerEngine::v8));
  init_params.set_require_code_preload_for_execution(false);
  init_params.set_compilation_context_cache_size(5);
  init_params.set_native_js_function_comms_fd(-1);
  init_params.mutable_native_js_function_names()->Clear();
  init_params.set_js_engine_initial_heap_size_mb(0);
  init_params.set_js_engine_maximum_heap_size_mb(0);
  init_params.set_js_engine_max_wasm_memory_number_of_pages(0);
  return init_params;
}

TEST(WorkerWrapperTest, CanRunCodeThroughWrapperWithoutPreload) {
  auto init_params = GetDefaultInitParams();
  std::string serialized_init_params;
  ASSERT_TRUE(init_params.SerializeToString(&serialized_init_params));

  sapi::LenValStruct sapi_init_params(
      serialized_init_params.size(),
      static_cast<void*>(serialized_init_params.data()));

  auto result = ::InitFromSerializedData(&sapi_init_params);
  EXPECT_EQ(SC_OK, result);

  result = ::Run();
  EXPECT_EQ(SC_OK, result);

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(
      "function cool_func() { return \"Hi there from JS :)\" }");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  std::string serialized_worker_params;
  ASSERT_TRUE(params_proto.SerializeToString(&serialized_worker_params));
  // We need to copy the serialized proto because RunCodeFromSerializedData()
  // will take ownership of the data that it contains and we don't want to free
  // the same memory twice.
  std::unique_ptr<unsigned char[]> worker_params_data(
      new unsigned char[serialized_worker_params.size()]);
  memcpy(worker_params_data.get(), serialized_worker_params.data(),
         serialized_worker_params.length());

  sapi::LenValStruct sapi_worker_params(
      serialized_worker_params.size(),
      static_cast<void*>(worker_params_data.release()));

  result = ::RunCodeFromSerializedData(&sapi_worker_params);
  ASSERT_EQ(SC_OK, result);

  // Take ownership of the response bytes, these will have been malloc'd by
  // RunCodeFromSerializedData() if it was successful.
  std::unique_ptr<unsigned char[]> response_bytes(
      static_cast<unsigned char*>(sapi_worker_params.data));

  ::worker_api::WorkerParamsProto response_proto;
  ASSERT_TRUE(response_proto.ParseFromArray(sapi_worker_params.data,
                                            sapi_worker_params.size));
  EXPECT_EQ(response_proto.response(), "\"Hi there from JS :)\"");

  result = ::Stop();
  EXPECT_EQ(SC_OK, result);
}

TEST(WorkerWrapperTest, FailsToRunCodeWhenPreloadIsRequiredAndExecuteIsSent) {
  auto init_params = GetDefaultInitParams();
  init_params.set_require_code_preload_for_execution(true);
  init_params.set_worker_factory_js_engine(
      static_cast<int>(worker::WorkerFactory::WorkerEngine::v8));
  init_params.set_require_code_preload_for_execution(true);

  std::string serialized_init_params;
  ASSERT_TRUE(init_params.SerializeToString(&serialized_init_params));

  sapi::LenValStruct sapi_init_params(
      serialized_init_params.size(),
      static_cast<void*>(serialized_init_params.data()));

  auto result = ::InitFromSerializedData(&sapi_init_params);
  EXPECT_EQ(SC_OK, result);

  result = ::Run();
  EXPECT_EQ(SC_OK, result);

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(
      "function cool_func() { return \"Hi there from JS :)\" }");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  std::string serialized_worker_params;
  ASSERT_TRUE(params_proto.SerializeToString(&serialized_worker_params));

  sapi::LenValStruct sapi_worker_params(
      serialized_worker_params.size(),
      static_cast<void*>(serialized_worker_params.data()));

  result = ::RunCodeFromSerializedData(&sapi_worker_params);
  EXPECT_NE(SC_OK, result);

  result = ::Stop();
  EXPECT_EQ(SC_OK, result);
}
}  // namespace google::scp::roma::sandbox::worker_api::test
