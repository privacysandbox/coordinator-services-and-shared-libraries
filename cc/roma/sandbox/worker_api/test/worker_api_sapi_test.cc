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

#include "roma/sandbox/worker_api/src/worker_api_sapi.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "public/core/test/interface/execution_result_matchers.h"
#include "roma/sandbox/constants/constants.h"

using google::scp::roma::sandbox::constants::kCodeVersion;
using google::scp::roma::sandbox::constants::
    kExecutionMetricSandboxedJsEngineCallNs;
using google::scp::roma::sandbox::constants::kHandlerName;
using google::scp::roma::sandbox::constants::kRequestAction;
using google::scp::roma::sandbox::constants::kRequestActionExecute;
using google::scp::roma::sandbox::constants::kRequestType;
using google::scp::roma::sandbox::constants::kRequestTypeJavascript;
using std::string;
using std::vector;

namespace google::scp::roma::sandbox::worker_api::test {

static WorkerApiSapiConfig GetDefaultConfig() {
  WorkerApiSapiConfig config;
  config.worker_js_engine = worker::WorkerFactory::WorkerEngine::v8;
  config.js_engine_require_code_preload = false;
  config.compilation_context_cache_size = 5;
  config.native_js_function_comms_fd = -1;
  config.native_js_function_names = vector<string>();
  config.max_worker_virtual_memory_mb = 0;
  config.js_engine_resource_constraints.initial_heap_size_in_mb = 0;
  config.js_engine_resource_constraints.maximum_heap_size_in_mb = 0;
  config.js_engine_max_wasm_memory_number_of_pages = 0;
  return config;
}

TEST(WorkerApiSapiTest, WorkerWorksThroughSandbox) {
  auto config = GetDefaultConfig();
  WorkerApiSapi worker_api(config);

  auto result = worker_api.Init();
  EXPECT_SUCCESS(result);

  result = worker_api.Run();
  EXPECT_SUCCESS(result);

  WorkerApi::RunCodeRequest request = {
      .code = "function hello_world() { return \"World. Hello!\" }",
      .metadata = {{kRequestType, kRequestTypeJavascript},
                   {kHandlerName, "hello_world"},
                   {kCodeVersion, "1"},
                   {kRequestAction, kRequestActionExecute}}};

  auto response_or = worker_api.RunCode(request);

  EXPECT_SUCCESS(response_or.result());
  EXPECT_EQ(*response_or->response, "\"World. Hello!\"");
}

TEST(WorkerApiSapiTest, WorkerWithInputsWorksThroughSandbox) {
  auto config = GetDefaultConfig();
  WorkerApiSapi worker_api(config);

  auto result = worker_api.Init();
  EXPECT_SUCCESS(result);

  result = worker_api.Run();
  EXPECT_SUCCESS(result);

  WorkerApi::RunCodeRequest request = {
      .code =
          "function func(input1, input2) { return input1 + \" \" + input2 }",
      .input = {"\"pos0 string\"", "\"pos1 string\""},
      .metadata = {{kRequestType, kRequestTypeJavascript},
                   {kHandlerName, "func"},
                   {kCodeVersion, "1"},
                   {kRequestAction, kRequestActionExecute}}};

  auto response_or = worker_api.RunCode(request);

  EXPECT_SUCCESS(response_or.result());
  EXPECT_EQ(*response_or->response, "\"pos0 string pos1 string\"");
}

TEST(WorkerApiSapiTest, ShouldGetExecutionMetrics) {
  auto config = GetDefaultConfig();
  WorkerApiSapi worker_api(config);

  auto result = worker_api.Init();
  EXPECT_SUCCESS(result);

  result = worker_api.Run();
  EXPECT_SUCCESS(result);

  WorkerApi::RunCodeRequest request = {
      .code =
          "function func(input1, input2) { return input1 + \" \" + input2 }",
      .input = {"\"pos0 string\"", "\"pos1 string\""},
      .metadata = {{kRequestType, kRequestTypeJavascript},
                   {kHandlerName, "func"},
                   {kCodeVersion, "1"},
                   {kRequestAction, kRequestActionExecute}}};

  auto response_or = worker_api.RunCode(request);

  EXPECT_SUCCESS(response_or.result());
  EXPECT_EQ(*response_or->response, "\"pos0 string pos1 string\"");

  EXPECT_GT(response_or->metrics.at(kExecutionMetricSandboxedJsEngineCallNs),
            0);
}
}  // namespace google::scp::roma::sandbox::worker_api::test
