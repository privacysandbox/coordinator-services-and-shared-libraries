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
#include "roma/ipc/src/ipc_message.h"

#include "error_codes.h"

namespace google::scp::roma::worker {

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
      const common::RomaString& js, common::RomaString& err_msg,
      v8::Local<v8::UnboundScript>* unbound_script = nullptr) noexcept;

  /**
   * @brief Get JS handler from context.
   *
   * @param handler_name the name of the handler.
   * @param handler the handler of the code object.
   * @param err_msg the error message to output.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult GetJsHandler(
      const common::RomaString& handler_name, v8::Local<v8::Value>& handler,
      common::RomaString& err_msg) noexcept;

  /**
   * @brief Compiles and runs WASM code object.
   *
   * @param wasm the byte object of WASM code.
   * @param err_msg the error message to output.
   * @return core::ExecutionResult the execution result of JavaScript code
   * object compile and run.
   */
  static core::ExecutionResult CompileRunWASM(
      const common::RomaString& wasm, common::RomaString& err_msg) noexcept;

  /**
   * @brief Get handler from WASM export object.
   *
   * @param handler_name the name of the handler.
   * @param handler the handler of the code object.
   * @param err_msg the error message to output.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult GetWasmHandler(
      const common::RomaString& handler_name, v8::Local<v8::Value>& handler,
      common::RomaString& err_msg) noexcept;

  /**
   * @brief Reports the caught exception from v8 isolate to error
   * message, and returns associated execution result.
   *
   * @param try_catch the pointer to v8 try catch object.
   * @param err_msg the reference to error message.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult ReportException(
      v8::TryCatch* try_catch, common::RomaString& err_msg) noexcept;

  /**
   * @brief Converts RomaString to v8 local native string.
   *
   * @param roma_string The object of RomaString.
   * @param v8_string The output of v8 local string.
   * @return true
   * @return false
   */
  static bool RomaStrToLocalStr(const common::RomaString& roma_string,
                                v8::Local<v8::String>& v8_string) noexcept;

  /**
   * @brief Converts RomaVector to v8 Array.
   *
   * @param input The object of RomaVector.
   * @param is_wasm Whether this is targeted towards a wasm handler.
   * @return v8::Local<v8::String> The output of v8 Value.
   */
  static v8::Local<v8::Array> InputToLocalArgv(
      const common::RomaVector<common::RomaString>& input,
      bool is_wasm = false) noexcept;

  /**
   * @brief Gets the execution result based on the exception_result and
   * predefined_result. Returns the exception_result if it is defined.
   * Otherwise, return the predefined_result.
   *
   * @param exception_result The exception_result from ReportException().
   * @param defined_code The pre-defined status code.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult GetExecutionResult(
      const core::ExecutionResult& exception_result,
      core::StatusCode defined_code) noexcept;

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
};
}  // namespace google::scp::roma::worker
