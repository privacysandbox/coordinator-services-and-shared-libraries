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
#include "include/libplatform/libplatform.h"
#include "include/v8.h"
#include "public/core/interface/execution_result.h"
#include "roma/interface/roma.h"
#include "roma/sandbox/js_engine/src/js_engine.h"
#include "roma/sandbox/worker/src/worker_utils.h"
#include "roma/worker/src/execution_utils.h"
#include "roma/worker/src/execution_watchdog.h"

#include "error_codes.h"
#include "snapshot_compilation_context.h"
#include "v8_isolate_visitor.h"

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {
/**
 * @brief Implementation of a JS engine using v8
 *
 */
class V8JsEngine : public JsEngine {
 public:
  V8JsEngine(
      const std::vector<std::shared_ptr<V8IsolateVisitor>>& isolate_visitors =
          std::vector<std::shared_ptr<V8IsolateVisitor>>(),
      const JsEngineResourceConstraints& v8_resource_constraints =
          JsEngineResourceConstraints())
      : isolate_visitors_(isolate_visitors),
        v8_resource_constraints_(v8_resource_constraints),
        execution_watchdog_(
            std::make_unique<roma::worker::ExecutionWatchDog>()) {
    for (const auto& visitor : isolate_visitors_) {
      visitor->AddExternalReferences(external_references_);
    }
    // Must be null terminated
    external_references_.push_back(0);
  }

  core::ExecutionResult Init() noexcept override;

  core::ExecutionResult Run() noexcept override;

  core::ExecutionResult Stop() noexcept override;

  core::ExecutionResult OneTimeSetup(
      const absl::flat_hash_map<std::string, std::string>& config =
          absl::flat_hash_map<std::string, std::string>()) noexcept override;

  core::ExecutionResultOr<js_engine::JsEngineExecutionResponse> CompileAndRunJs(
      const std::string& code, const std::string& function_name,
      const std::vector<absl::string_view>& input,
      const absl::flat_hash_map<std::string, std::string>& metadata,
      const js_engine::RomaJsEngineCompilationContext& context =
          RomaJsEngineCompilationContext()) noexcept override;

  core::ExecutionResultOr<js_engine::JsEngineExecutionResponse>
  CompileAndRunJsWithWasm(
      const std::string& code, const absl::Span<const std::uint8_t>& wasm,
      const std::string& function_name,
      const std::vector<absl::string_view>& input,
      const absl::flat_hash_map<std::string, std::string>& metadata,
      const js_engine::RomaJsEngineCompilationContext& context =
          RomaJsEngineCompilationContext()) noexcept override;

  core::ExecutionResultOr<js_engine::JsEngineExecutionResponse>
  CompileAndRunWasm(
      const std::string& code, const std::string& function_name,
      const std::vector<absl::string_view>& input,
      const absl::flat_hash_map<std::string, std::string>& metadata,
      const js_engine::RomaJsEngineCompilationContext& context =
          RomaJsEngineCompilationContext()) noexcept override;

 private:
  /**
   * @brief Create a Snapshot object
   *
   * @param startup_data
   * @param js_code
   * @param err_msg
   * @return core::ExecutionResult
   */
  core::ExecutionResult CreateSnapshot(v8::StartupData& startup_data,
                                       const std::string& js_code,
                                       std::string& err_msg) noexcept;
  /**
   * @brief Create a Snapshot object with start up data containing global
   * objects that can be directly referenced in the JS code.
   *
   * @param startup_data
   * @param wasm
   * @param metadata
   * @param err_msg
   * @return core::ExecutionResult
   */
  core::ExecutionResult CreateSanpshotWithGlobals(
      v8::StartupData& startup_data, const absl::Span<const std::uint8_t>& wasm,
      const absl::flat_hash_map<std::string, std::string>& metadata,
      std::string& err_msg) noexcept;
  /**
   * @brief Create a Compilation Context object which wraps a object of
   * SnapshotCompilationContext in the context.
   *
   * @param code
   * @param wasm
   * @param metadata
   * @param err_msg
   * @return
   * core::ExecutionResultOr<js_engine::RomaJsEngineCompilationContext>
   */
  core::ExecutionResultOr<js_engine::RomaJsEngineCompilationContext>
  CreateCompilationContext(
      const std::string& code, const absl::Span<const std::uint8_t>& wasm,
      const absl::flat_hash_map<std::string, std::string>& metadata,
      std::string& err_msg) noexcept;

  /// @brief Create a v8 isolate instance.
  virtual core::ExecutionResultOr<v8::Isolate*> CreateIsolate(
      const v8::StartupData& startup_data = {nullptr, 0}) noexcept;

  /// @brief Dispose v8 isolate.
  virtual void DisposeIsolate() noexcept;

  /**
   * @brief Start timing the execution running in the isolate with watchdog.
   *
   * @param isolate the target isolate where the execution is running.
   * @param metadata metadata from the request which may contain a
   * kTimeoutMsTag with the timeout value. If there is no kTimeoutMsTag, the
   * default timeout value kDefaultExecutionTimeoutMs will be used.
   */
  void StartWatchdogTimer(
      v8::Isolate* isolate,
      const absl::flat_hash_map<std::string, std::string>& metadata) noexcept;
  /**
   * @brief Stop the timer for the execution in isolate. Call this function
   * after execution is complete to avoid watchdog termination of standby
   * isolate.
   *
   */
  void StopWatchdogTimer() noexcept;

  /**
   * @brief Execute invocation request in current compilation context.
   *
   * @param current_compilation_context
   * @param function_name
   * @param input
   * @param metadata
   * @return core::ExecutionResultOr<ExecutionResponse>
   */
  core::ExecutionResultOr<ExecutionResponse> ExecuteJs(
      const std::shared_ptr<SnapshotCompilationContext>&
          current_compilation_context,
      const std::string& function_name,
      const std::vector<absl::string_view>& input,
      const absl::flat_hash_map<std::string, std::string>& metadata) noexcept;

  /**
   * @brief Compile the wasm code array as a wasm module.
   *
   * @param isolate
   * @param wasm
   * @param err_msg
   * @return core::ExecutionResult
   */
  core::ExecutionResult CompileWasmCodeArray(
      v8::Isolate* isolate, const absl::Span<const std::uint8_t>& wasm,
      std::string& err_msg) noexcept;

  /**
   * @brief Initialize and run a execution watchdog for current v8_isolate.
   *
   * @return core::ExecutionResult
   */
  core::ExecutionResult InitAndRunWatchdog() noexcept;

  v8::Isolate* v8_isolate_ = nullptr;
  const std::vector<std::shared_ptr<V8IsolateVisitor>> isolate_visitors_;

  /// @brief These are external references (pointers to data outside of the
  /// v8 heap) which are needed for serialization of the v8 snapshot.
  std::vector<intptr_t> external_references_;

  /// v8 heap resource constraints.
  const JsEngineResourceConstraints v8_resource_constraints_;
  /// @brief A timer thread watches the code execution in v8 isolate and
  /// timeouts the execution in set time.
  std::unique_ptr<roma::worker::ExecutionWatchDog> execution_watchdog_{nullptr};
};
}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine
