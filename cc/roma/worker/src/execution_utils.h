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

#pragma once

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "include/v8.h"
#include "public/core/interface/execution_result.h"
#include "roma/config/src/type_converter.h"
#include "roma/interface/roma.h"
#include "roma/wasm/src/deserializer.h"
#include "roma/wasm/src/serializer.h"
#include "roma/wasm/src/wasm_types.h"

#include "error_codes.h"

namespace google::scp::roma::worker {

static constexpr char kWebAssemblyTag[] = "WebAssembly";
static constexpr char kInstanceTag[] = "Instance";
static constexpr char kExportsTag[] = "exports";
static constexpr char kRegisteredWasmExports[] = "RomaRegisteredWasmExports";
static constexpr char kTimeoutErrorMsg[] = "execution timeout";

class ExecutionUtils {
 public:
  /**
   * @brief Compiles and runs JavaScript code object.
   *
   * @param js the string object of JavaScript code.
   * @param err_msg
   * @param[out] unbound_script this is optional output. If unbound_script is
   * provided, a local UnboundScript will be assigned to unbound_script.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult CompileRunJS(
      const std::string& js, std::string& err_msg,
      v8::Local<v8::UnboundScript>* unbound_script = nullptr) noexcept;

  /**
   * @brief Get JS handler from context.
   *
   * @param handler_name the name of the handler.
   * @param handler the handler of the code object.
   * @param err_msg the error message to output.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult GetJsHandler(const std::string& handler_name,
                                            v8::Local<v8::Value>& handler,
                                            std::string& err_msg) noexcept;

  /**
   * @brief Compiles and runs WASM code object.
   *
   * @param wasm the byte object of WASM code.
   * @param err_msg the error message to output.
   * @return core::ExecutionResult the execution result of JavaScript code
   * object compile and run.
   */
  static core::ExecutionResult CompileRunWASM(const std::string& wasm,
                                              std::string& err_msg) noexcept;

  /**
   * @brief Get handler from WASM export object.
   *
   * @param handler_name the name of the handler.
   * @param handler the handler of the code object.
   * @param err_msg the error message to output.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult GetWasmHandler(const std::string& handler_name,
                                              v8::Local<v8::Value>& handler,
                                              std::string& err_msg) noexcept;

  /**
   * @brief Converts string vector to v8 Array.
   *
   * @param input The object of std::vector<std::string>.
   * @param is_wasm Whether this is targeted towards a wasm handler.
   * @return v8::Local<v8::String> The output of v8 Value.
   */
  static v8::Local<v8::Array> InputToLocalArgv(
      const std::vector<std::string_view>& input,
      bool is_wasm = false) noexcept;

  /**
   * @brief Read a value from WASM memory
   *
   * @param isolate
   * @param context
   * @param offset
   * @param read_value_type The type of the WASM value being read
   * @return v8::Local<v8::Value>
   */
  static v8::Local<v8::Value> ReadFromWasmMemory(
      v8::Isolate* isolate, v8::Local<v8::Context>& context, int32_t offset,
      WasmDataType read_value_type);

  /**
   * @brief Extract the error message from v8::Message object.
   *
   * @param isolate
   * @param message
   * @return std::string
   */
  static std::string ExtractMessage(v8::Isolate* isolate,
                                    v8::Local<v8::Message> message) noexcept;

  /**
   * @brief Parse the input using JSON::Parse to turn it into the right JS types
   *
   * @param input
   * @return Local<Array> The array of parsed values
   */
  static v8::Local<v8::Array> ParseAsJsInput(
      const std::vector<std::string_view>& input);

  /**
   * @brief Parse the handler input to be provided to a WASM handler.
   * This function handles writing to the WASM memory if necessary.
   *
   * @param isolate
   * @param context
   * @param input
   * @return Local<Array> The input arguments to be provided to the WASM
   * handler.
   */
  static v8::Local<v8::Array> ParseAsWasmInput(
      v8::Isolate* isolate, v8::Local<v8::Context>& context,
      const std::vector<std::string_view>& input);

  /**
   * @brief Function that is used as the entry point to call user-provided
   * C++ binding functions.
   *
   * @param info
   */
  static void GlobalV8FunctionCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info);

  /**
   * @brief Check if err_msg contains a WebAssembly ReferenceError.
   *
   * @param err_msg
   * @return true
   * @return false
   */
  static bool CheckErrorWithWebAssembly(std::string& err_msg) noexcept {
    return err_msg.find(kJsWasmMixedError) != std::string::npos;
  }

  /**
   * @brief Create an Unbound Script object.
   *
   * @param js
   * @param err_msg
   * @return core::ExecutionResult
   */
  static core::ExecutionResult CreateUnboundScript(
      v8::Global<v8::UnboundScript>& unbound_script, v8::Isolate* isolate,
      const std::string& js, std::string& err_msg) noexcept;

  /**
   * @brief Bind UnboundScript to current context and run it.
   *
   * @param err_msg
   * @return core::ExecutionResult
   */
  static core::ExecutionResult BindUnboundScript(
      const v8::Global<v8::UnboundScript>& global_unbound_script,
      std::string& err_msg) noexcept;

  /**
   * @brief Generate an object that represents the WASM imports modules
   *
   * @param isolate
   * @return Local<Object>
   */
  static v8::Local<v8::Object> GenerateWasmImports(v8::Isolate* isolate);

  static std::string DescribeError(v8::Isolate* isolate,
                                   v8::TryCatch* try_catch) noexcept;

  /**
   * @brief Get the WASM memory object that was registered in the global context
   *
   * @param isolate
   * @param context
   * @return Local<Value> The WASM memory object
   */
  static v8::Local<v8::Value> GetWasmMemoryObject(
      v8::Isolate* isolate, v8::Local<v8::Context>& context);

  static core::ExecutionResult V8PromiseHandler(v8::Isolate* isolate,
                                                v8::Local<v8::Value>& result,
                                                std::string& err_msg);

 private:
  static constexpr char kJsWasmMixedError[] =
      "ReferenceError: WebAssembly is not defined";
};
}  // namespace google::scp::roma::worker
