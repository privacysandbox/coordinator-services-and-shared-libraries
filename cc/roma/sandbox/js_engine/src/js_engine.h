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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::roma::sandbox::js_engine {
/**
 * @brief Context that can be used to load contexts to the JS engine.
 *
 */
struct RomaJsEngineCompilationContext {
  RomaJsEngineCompilationContext() { has_context = false; }

  bool has_context;
  std::shared_ptr<void> context;
};

struct ExecutionResponse {
  /// the response of handler function execution.
  std::shared_ptr<std::string> response = std::make_shared<std::string>();

  /// the metrics for handler function execution.
  absl::flat_hash_map<std::string, int64_t> metrics;
};

/**
 * @brief The response returned by the JS engine on a code run.
 *
 */
struct JsEngineExecutionResponse {
  /**
   * The compilation context that was generated when compiling the code
   */
  RomaJsEngineCompilationContext compilation_context;

  /**
   * The response of the JS/WASM execution
   */
  ExecutionResponse execution_response;
};

/**
 * @brief Interface for a JS engine.
 *
 */
class JsEngine : public core::ServiceInterface {
 public:
  /**
   * Function that is intended to be called one at the beginning for any
   * one-time setup that is needed.
   */
  virtual core::ExecutionResult OneTimeSetup(
      const absl::flat_hash_map<std::string, std::string>& config) noexcept = 0;

  /**
   * @brief Builds and runs the JS code provided as input, and returns a context
   * which could be used to avoid recompilation. The context itself is optional.
   * @param code The code to compile and run (optional if context is valid)
   * @param function_name The name of the JS function to invoke
   * @param input The input to pass to the code
   * @param metadata The metadata associated with the code request.
   * @param context A context which could be used to avoid recompiling the JS
   * code
   * @return Whether the operation succeeded or failed, and a result object
   * which contains the response from the JS code and a compilation context
   * which could be used to skip compilation of the same code.
   */
  virtual core::ExecutionResultOr<JsEngineExecutionResponse> CompileAndRunJs(
      const std::string& code, const std::string& function_name,
      const std::vector<absl::string_view>& input,
      const absl::flat_hash_map<std::string, std::string>& metadata,
      const RomaJsEngineCompilationContext& context) noexcept = 0;

  /**
   * @brief Builds and runs the WASM binary provided as input, and returns a
   * context which could be used to avoid recompilation. The context itself is
   * optional.
   * @param code The code to compile and run (optional if context is valid)
   * @param function_name The name of the WASM function to invoke
   * @param input The input to pass to the code
   * @param metadata The metadata associated with the code request.
   * @param context A context which could be used to avoid recompiling the JS
   * code
   * @return Whether the operation succeeded or failed, and a result object
   * which contains the response from the WASM code and a compilation context
   * which could be used to skip compilation of the same code.
   */
  virtual core::ExecutionResultOr<JsEngineExecutionResponse> CompileAndRunWasm(
      const std::string& code, const std::string& function_name,
      const std::vector<absl::string_view>& input,
      const absl::flat_hash_map<std::string, std::string>& metadata,
      const RomaJsEngineCompilationContext& context) noexcept = 0;

  /**
   * @brief Builds and runs the JS code provided as input, loads the provided
   * WASM in the global namespace so that it can be instantiated from the JS
   * code, and returns a context which could be used to avoid recompilation. The
   * context itself is optional.
   * @param code The code to compile and run (optional if context is valid)
   * @param wasm The wasm module needed to be loaded by the JS code (optional if
   * context is valid).
   * @param function_name The name of the JS function to invoke
   * @param input The input to pass to the code
   * @param metadata The metadata associated with the code request.
   * @param context A context which could be used to avoid recompiling the JS
   * code
   * @return Whether the operation succeeded or failed, and a result object
   * which contains the response from the JS code and a compilation context
   * which could be used to skip compilation of the same code.
   */
  virtual core::ExecutionResultOr<JsEngineExecutionResponse>
  CompileAndRunJsWithWasm(
      const std::string& code, const absl::Span<const std::uint8_t>& wasm,
      const std::string& function_name,
      const std::vector<absl::string_view>& input,
      const absl::flat_hash_map<std::string, std::string>& metadata,
      const RomaJsEngineCompilationContext& context) noexcept = 0;
};
}  // namespace google::scp::roma::sandbox::js_engine
