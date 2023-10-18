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

#include "roma/sandbox/js_engine/src/v8_engine/v8_js_engine.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "core/test/utils/auto_init_run_stop.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "roma/wasm/test/testing_utils.h"

using absl::flat_hash_map;
using absl::Span;
using absl::string_view;
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::SC_ROMA_V8_ENGINE_COULD_NOT_PARSE_SCRIPT_INPUT;
using google::scp::core::errors::SC_ROMA_V8_ENGINE_ERROR_INVOKING_HANDLER;
using google::scp::core::errors::SC_ROMA_V8_ENGINE_EXECUTION_TIMEOUT;
using google::scp::core::errors::SC_ROMA_V8_WORKER_CODE_COMPILE_FAILURE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_SCRIPT_RUN_FAILURE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_WASM_COMPILE_FAILURE;
using google::scp::core::test::AutoInitRunStop;
using google::scp::core::test::ResultIs;
using google::scp::roma::kDefaultExecutionTimeoutMs;
using google::scp::roma::kTimeoutMsTag;
using google::scp::roma::kWasmCodeArrayName;
using std::string;
using std::vector;

using google::scp::roma::sandbox::js_engine::v8_js_engine::V8JsEngine;
using google::scp::roma::wasm::testing::WasmTestingUtils;

namespace google::scp::roma::sandbox::js_engine::test {
class V8JsEngineTest : public ::testing::Test {
 public:
  static void SetUpTestSuite() {
    V8JsEngine engine;
    engine.OneTimeSetup();
  }
};

TEST_F(V8JsEngineTest, CanRunJsCode) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  auto js_code =
      "function hello_js(input1, input2) { return \"Hello World!\" + \" \" + "
      "input1 + \" \" + input2 }";
  vector<string_view> input = {"\"vec input 1\"", "\"vec input 2\""};
  auto response_or =
      engine.CompileAndRunJs(js_code, "hello_js", input, {} /*metadata*/);

  EXPECT_SUCCESS(response_or.result());
  auto response_string = *response_or->execution_response.response;
  EXPECT_EQ(response_string, "\"Hello World! vec input 1 vec input 2\"");
}

TEST_F(V8JsEngineTest, CanRunAsyncJsCodeReturningPromiseExplicitly) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  auto js_code = R"JS_CODE(
      function sleep(milliseconds) {
        const date = Date.now();
        let currentDate = null;
        do {
          currentDate = Date.now();
        } while (currentDate - date < milliseconds);
      }

      function resolveAfterOneSecond() {
        return new Promise((resolve) => {
          sleep(1000);
          resolve("some cool string");
        });
      }

      function Handler() {
          return resolveAfterOneSecond();
      }
    )JS_CODE";
  auto response_or =
      engine.CompileAndRunJs(js_code, "Handler", {} /*input*/, {} /*metadata*/);

  EXPECT_SUCCESS(response_or.result());
  auto response_string = *response_or->execution_response.response;
  EXPECT_EQ(response_string, "\"some cool string\"");
}

TEST_F(V8JsEngineTest, CanRunAsyncJsCodeReturningPromiseImplicitly) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  auto js_code = R"JS_CODE(
      function sleep(milliseconds) {
        const date = Date.now();
        let currentDate = null;
        do {
          currentDate = Date.now();
        } while (currentDate - date < milliseconds);
      }

      function resolveAfterOneSecond() {
        return new Promise((resolve) => {
          sleep(1000);
          resolve("some cool string");
        });
      }

      async function Handler() {
          result = await resolveAfterOneSecond();
          return result;
      }
    )JS_CODE";
  auto response_or =
      engine.CompileAndRunJs(js_code, "Handler", {} /*input*/, {} /*metadata*/);

  EXPECT_SUCCESS(response_or.result());
  auto response_string = *response_or->execution_response.response;
  EXPECT_EQ(response_string, "\"some cool string\"");
}

TEST_F(V8JsEngineTest, CanHandlePromiseRejectionInAsyncJs) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  auto js_code = R"JS_CODE(
      function sleep(milliseconds) {
        const date = Date.now();
        let currentDate = null;
        do {
          currentDate = Date.now();
        } while (currentDate - date < milliseconds);
      }

      function resolveAfterOneSecond() {
        return new Promise((resolve, reject) => {
          sleep(1000);
          reject("some cool string");
        });
      }

      async function Handler() {
          result = await resolveAfterOneSecond();
          return result;
      }
    )JS_CODE";
  auto response_or =
      engine.CompileAndRunJs(js_code, "Handler", {} /*input*/, {} /*metadata*/);

  EXPECT_FALSE(response_or.result().Successful());
}

TEST_F(V8JsEngineTest, CanHandleCompilationFailures) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  auto js_code = "function hello_js(input1, input2) {";
  vector<string_view> input = {"\"vec input 1\"", "\"vec input 2\""};
  auto response_or =
      engine.CompileAndRunJs(js_code, "hello_js", input, {} /*metadata*/);

  EXPECT_THAT(
      response_or.result(),
      ResultIs(FailureExecutionResult(SC_ROMA_V8_WORKER_CODE_COMPILE_FAILURE)));
}

TEST_F(V8JsEngineTest, CanRunCodeRequestWithJsonInput) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  auto js_code =
      "function Handler(a, b) { "
      "return (a[\"value\"] + b[\"value\"]); }";
  vector<string_view> input = {"{\"value\":1}", "{\"value\":2}"};
  auto response_or =
      engine.CompileAndRunJs(js_code, "Handler", input, {} /*metadata*/);

  EXPECT_SUCCESS(response_or.result());
  auto response_string = *response_or->execution_response.response;
  EXPECT_EQ(response_string, "3");
}

TEST_F(V8JsEngineTest, ShouldFailIfInputIsBadJsonInput) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  auto js_code =
      "function Handler(a, b) { "
      "return (a[\"value\"] + b[\"value\"]); }";
  vector<string_view> input = {"value\":1}", "{\"value\":2}"};
  auto response_or =
      engine.CompileAndRunJs(js_code, "Handler", input, {} /*metadata*/);

  EXPECT_THAT(response_or.result(),
              ResultIs(FailureExecutionResult(
                  SC_ROMA_V8_ENGINE_COULD_NOT_PARSE_SCRIPT_INPUT)));
}

TEST_F(V8JsEngineTest, ShouldSucceedWithEmptyResponseIfHandlerNameIsEmpty) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  auto js_code =
      "function hello_js(input1, input2) { return \"Hello World!\" + \" \" + "
      "input1 + \" \" + input2 }";
  vector<string_view> input = {"\"vec input 1\"", "\"vec input 2\""};

  // Empty handler
  auto response_or =
      engine.CompileAndRunJs(js_code, "", input, {} /*metadata*/);

  EXPECT_SUCCESS(response_or.result());
  auto response_string = *response_or->execution_response.response;
  EXPECT_EQ(response_string, "");
}

TEST_F(V8JsEngineTest, ShouldFailIfInputCannotBeParsed) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  auto js_code =
      "function hello_js(input1, input2) { return \"Hello World!\" + \" \" + "
      "input1 + \" \" + input2 }";
  // Bad input
  vector<string_view> input = {"vec input 1\"", "\"vec input 2\""};

  auto response_or =
      engine.CompileAndRunJs(js_code, "hello_js", input, {} /*metadata*/);

  EXPECT_THAT(response_or.result(),
              ResultIs(FailureExecutionResult(
                  SC_ROMA_V8_ENGINE_COULD_NOT_PARSE_SCRIPT_INPUT)));
}

TEST_F(V8JsEngineTest, ShouldFailIfHandlerIsNotFound) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  auto js_code =
      "function hello_js(input1, input2) { return \"Hello World!\" + \" \" + "
      "input1 + \" \" + input2 }";
  vector<string_view> input = {"\"vec input 1\"", "\"vec input 2\""};

  auto response_or =
      engine.CompileAndRunJs(js_code, "not_found", input, {} /*metadata*/);

  EXPECT_FALSE(response_or.result());
}

TEST_F(V8JsEngineTest, CanRunWasmCode) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./cc/roma/testing/cpp_wasm_string_in_string_out_example/"
      "string_in_string_out.wasm");

  auto wasm_code =
      string(reinterpret_cast<char*>(wasm_bin.data()), wasm_bin.size());
  vector<string_view> input = {"\"Some input string\""};

  auto response_or =
      engine.CompileAndRunWasm(wasm_code, "Handler", input, {} /*metadata*/);

  EXPECT_SUCCESS(response_or.result());
  auto response_string = *response_or->execution_response.response;
  EXPECT_EQ(response_string, "\"Some input string Hello World from WASM\"");
}

TEST_F(V8JsEngineTest, WasmShouldSucceedWithEmptyResponseIfHandlerNameIsEmpty) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./cc/roma/testing/cpp_wasm_string_in_string_out_example/"
      "string_in_string_out.wasm");

  auto wasm_code =
      string(reinterpret_cast<char*>(wasm_bin.data()), wasm_bin.size());
  vector<string_view> input = {"\"Some input string\""};

  // Empty handler
  auto response_or =
      engine.CompileAndRunWasm(wasm_code, "", input, {} /*metadata*/);

  EXPECT_SUCCESS(response_or.result());
  auto response_string = *response_or->execution_response.response;
  EXPECT_EQ(response_string, "");
}

TEST_F(V8JsEngineTest, WasmShouldFailIfInputCannotBeParsed) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./cc/roma/testing/cpp_wasm_string_in_string_out_example/"
      "string_in_string_out.wasm");

  auto wasm_code =
      string(reinterpret_cast<char*>(wasm_bin.data()), wasm_bin.size());
  // Bad input
  vector<string_view> input = {"\"Some input string"};

  auto response_or =
      engine.CompileAndRunWasm(wasm_code, "Handler", input, {} /*metadata*/);

  EXPECT_FALSE(response_or.result().Successful());
}

TEST_F(V8JsEngineTest, WasmShouldFailIfBadWasm) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  // Modified wasm so it doesn't compile
  char wasm_bin[] = {0x07, 0x01, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03,
                     0x02, 0x01, 0x00, 0x07, 0x07, 0x01, 0x03, 0x61,
                     0x64, 0x64, 0x00, 0x00, 0x0a, 0x01, 0x07, 0x00,
                     0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b};

  auto wasm_code = string(reinterpret_cast<char*>(wasm_bin), sizeof(wasm_bin));
  // Bad input
  vector<string_view> input = {"\"Some input string\""};

  auto response_or =
      engine.CompileAndRunWasm(wasm_code, "Handler", input, {} /*metadata*/);

  EXPECT_FALSE(response_or.result().Successful());
}

TEST_F(V8JsEngineTest, CanTimeoutExecutionWithDefaultTimeoutValue) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  auto js_code = R"""(
    function hello_js() {
      while (true) {};
      return 0;
      }
  )""";
  vector<string_view> input;
  flat_hash_map<string, string> metadata;

  auto response_or =
      engine.CompileAndRunJs(js_code, "hello_js", input, metadata);

  EXPECT_THAT(
      response_or.result(),
      ResultIs(FailureExecutionResult(SC_ROMA_V8_ENGINE_EXECUTION_TIMEOUT)));
}

TEST_F(V8JsEngineTest, CanTimeoutExecutionWithCustomTimeoutTag) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  // This code will execute more than 200 milliseconds.
  auto js_code = R"""(
    function sleep(milliseconds) {
      const date = Date.now();
      let currentDate = null;
      do {
        currentDate = Date.now();
      } while (currentDate - date < milliseconds);
    }
    function hello_js() {
        sleep(200);
        return 0;
      }
  )""";
  vector<string_view> input;

  {
    flat_hash_map<string, string> metadata;
    // Set the timeout flag to 100 milliseconds. When it runs for more than 100
    // milliseconds, it times out.
    metadata[kTimeoutMsTag] = "100";

    auto response_or =
        engine.CompileAndRunJs(js_code, "hello_js", input, metadata);

    EXPECT_THAT(
        response_or.result(),
        ResultIs(FailureExecutionResult(SC_ROMA_V8_ENGINE_EXECUTION_TIMEOUT)));
  }

  {
    // Without a custom timeout tag and the default timeout value is 5 seconds,
    // the code executes successfully.
    auto response_or = engine.CompileAndRunJs(js_code, "hello_js", input, {});
    EXPECT_SUCCESS(response_or.result());
  }
}

TEST_F(V8JsEngineTest, JsMixedGlobalWasmCompileRunExecute) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  // JS code mixed with global WebAssembly variables.
  auto js_code = R"""(
          let bytes = new Uint8Array([
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
            0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
            0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
            0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b
          ]);
          let module = new WebAssembly.Module(bytes);
          let instance = new WebAssembly.Instance(module);
          function hello_js(a, b) {
            return instance.exports.add(a, b);
          }
        )""";

  {
    auto response_or = engine.CompileAndRunJs(js_code, "hello_js", {}, {});
    EXPECT_SUCCESS(response_or.result());
  }

  {
    vector<string_view> input = {"1", "2"};
    auto response_or = engine.CompileAndRunJs(js_code, "hello_js", input, {});
    EXPECT_SUCCESS(response_or.result());
    auto response_string = *response_or->execution_response.response;
    EXPECT_EQ(response_string, "3");
  }

  {
    vector<string_view> input = {"1", "6"};
    auto response_or = engine.CompileAndRunJs(js_code, "hello_js", input, {});
    EXPECT_SUCCESS(response_or.result());
    auto response_string = *response_or->execution_response.response;
    EXPECT_EQ(response_string, "7");
  }
}

TEST_F(V8JsEngineTest, JsMixedLocalWasmCompileRunExecute) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  // JS code mixed with local WebAssembly variables.
  auto js_code = R"""(
          let bytes = new Uint8Array([
            0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01,
            0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07,
            0x07, 0x01, 0x03, 0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
            0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b
          ]);
          function hello_js(a, b) {
            var module = new WebAssembly.Module(bytes);
            var instance = new WebAssembly.Instance(module);
            return instance.exports.add(a, b);
          }
        )""";

  {
    auto response_or = engine.CompileAndRunJs(js_code, "", {}, {});
    EXPECT_SUCCESS(response_or.result());
  }

  {
    vector<string_view> input = {"1", "2"};
    auto response_or = engine.CompileAndRunJs(js_code, "hello_js", input, {});
    EXPECT_SUCCESS(response_or.result());
    auto response_string = *response_or->execution_response.response;
    EXPECT_EQ(response_string, "3");
  }

  {
    vector<string_view> input = {"1", "6"};
    auto response_or = engine.CompileAndRunJs(js_code, "hello_js", input, {});
    EXPECT_SUCCESS(response_or.result());
    auto response_string = *response_or->execution_response.response;
    EXPECT_EQ(response_string, "7");
  }
}

TEST_F(V8JsEngineTest, JsWithWasmCompileRunExecute) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  // JS code mixed with local WebAssembly variables.
  auto js_code = R"""(
          let module = new WebAssembly.Module(addModule);
          let instance = new WebAssembly.Instance(module);
          function hello_js(a, b) {
            return instance.exports.add(a, b);
          }
        )""";
  vector<uint8_t> wasm_bin{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01,
                           0x07, 0x01, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x03,
                           0x02, 0x01, 0x00, 0x07, 0x07, 0x01, 0x03, 0x61, 0x64,
                           0x64, 0x00, 0x00, 0x0a, 0x09, 0x01, 0x07, 0x00, 0x20,
                           0x00, 0x20, 0x01, 0x6a, 0x0b};
  auto wasm = Span<const uint8_t>(wasm_bin);
  {
    auto response_or = engine.CompileAndRunJsWithWasm(
        js_code, wasm, "", {}, {{kWasmCodeArrayName, "addModule"}});
    EXPECT_SUCCESS(response_or.result());
  }
  {
    vector<string_view> input = {"1", "2"};
    auto response_or = engine.CompileAndRunJsWithWasm(
        js_code, wasm, "hello_js", input, {{kWasmCodeArrayName, "addModule"}});
    EXPECT_SUCCESS(response_or.result());
    auto response_string = *response_or->execution_response.response;
    EXPECT_EQ(response_string, "3");
  }
  {
    vector<string_view> input = {"1", "6"};
    auto response_or = engine.CompileAndRunJsWithWasm(
        js_code, wasm, "hello_js", input, {{kWasmCodeArrayName, "addModule"}});
    EXPECT_SUCCESS(response_or.result());
    auto response_string = *response_or->execution_response.response;
    EXPECT_EQ(response_string, "7");
  }
}

TEST_F(V8JsEngineTest, JsWithWasmCompileRunExecuteFailWithInvalidWasm) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  // JS code mixed with local WebAssembly variables.
  auto js_code = R"""(
          let module = new WebAssembly.Module(addModule);
          let instance = new WebAssembly.Instance(module);
          function hello_js(a, b) {
            return instance.exports.add(a, b);
          }
        )""";
  vector<uint8_t> wasm_bin{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
                           0x01, 0x02, 0x01, 0x00, 0x07, 0x07, 0x01, 0x03,
                           0x61, 0x64, 0x64, 0x00, 0x00, 0x0a, 0x09, 0x01,
                           0x07, 0x00, 0x20, 0x00, 0x20, 0x01, 0x6a, 0x0b};
  auto wasm = Span<const uint8_t>(wasm_bin);
  {
    auto response_or = engine.CompileAndRunJsWithWasm(
        js_code, wasm, "", {}, {{kWasmCodeArrayName, "addModule"}});
    EXPECT_THAT(response_or.result(),
                ResultIs(FailureExecutionResult(
                    SC_ROMA_V8_WORKER_WASM_COMPILE_FAILURE)));
  }

  {
    vector<string_view> input = {"1", "2"};
    auto response_or = engine.CompileAndRunJsWithWasm(
        js_code, wasm, "hello_js", input, {{kWasmCodeArrayName, "addModule"}});
    EXPECT_THAT(response_or.result(),
                ResultIs(FailureExecutionResult(
                    SC_ROMA_V8_WORKER_WASM_COMPILE_FAILURE)));
  }
}

TEST_F(V8JsEngineTest, JsWithWasmCompileRunExecuteWithWasiImports) {
  V8JsEngine engine;
  AutoInitRunStop to_handle_engine(engine);

  // JS code with wasm imports.
  auto js_code = R"""(
          const wasmImports = {
            wasi_snapshot_preview1: {
              proc_exit() {
                return;
              },
            },
          };
          let module = new WebAssembly.Module(testModule);
          let instance = new WebAssembly.Instance(module, wasmImports);
          function test_wasi(a) {
            return instance.exports.Handler(a);
          }
        )""";
  auto wasm_bin = WasmTestingUtils::LoadWasmFile(
      "./cc/roma/testing/cpp_wasi_dependency_example/wasi_dependency.wasm");
  vector<uint8_t> wasm_code(wasm_bin.begin(), wasm_bin.end());
  auto wasm = Span<const uint8_t>(wasm_code);
  {
    auto response_or = engine.CompileAndRunJsWithWasm(
        js_code, wasm, "", {}, {{kWasmCodeArrayName, "testModule"}});
    EXPECT_SUCCESS(response_or.result());
  }

  {
    vector<string_view> input = {"1"};
    auto response_or =
        engine.CompileAndRunJsWithWasm(js_code, wasm, "test_wasi", input,
                                       {{kWasmCodeArrayName, "testModule"}});
    EXPECT_SUCCESS(response_or.result());
    auto response_string = *response_or->execution_response.response;
    std::cout << "\n output: " << response_string << "\n";

    EXPECT_EQ(response_string, "0");
  }
  {
    vector<string_view> input = {"6"};
    auto response_or =
        engine.CompileAndRunJsWithWasm(js_code, wasm, "test_wasi", input,
                                       {{kWasmCodeArrayName, "testModule"}});
    EXPECT_SUCCESS(response_or.result());
    auto response_string = *response_or->execution_response.response;
    EXPECT_EQ(response_string, "1");
  }
}
}  // namespace google::scp::roma::sandbox::js_engine::test
