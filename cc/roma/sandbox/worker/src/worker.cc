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

#include "worker.h"

#include <unistd.h>

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "roma/logging/src/logging.h"
#include "roma/sandbox/constants/constants.h"
#include "roma/sandbox/worker/src/worker_utils.h"

using absl::flat_hash_map;
using absl::Span;
using absl::string_view;
using std::string;
using std::vector;

using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_ROMA_WORKER_MISSING_CONTEXT_WHEN_EXECUTING;
using google::scp::core::errors::SC_ROMA_WORKER_MISSING_METADATA_ITEM;
using google::scp::core::errors::SC_ROMA_WORKER_REQUEST_TYPE_NOT_SUPPORTED;
using google::scp::roma::sandbox::constants::kCodeVersion;
using google::scp::roma::sandbox::constants::kHandlerName;
using google::scp::roma::sandbox::constants::kRequestAction;
using google::scp::roma::sandbox::constants::kRequestActionExecute;
using google::scp::roma::sandbox::constants::kRequestActionLoad;
using google::scp::roma::sandbox::constants::kRequestType;
using google::scp::roma::sandbox::constants::kRequestTypeJavascript;
using google::scp::roma::sandbox::constants::kRequestTypeJavascriptWithWasm;
using google::scp::roma::sandbox::constants::kRequestTypeWasm;
using google::scp::roma::sandbox::js_engine::JsEngineExecutionResponse;
using google::scp::roma::sandbox::js_engine::RomaJsEngineCompilationContext;

namespace google::scp::roma::sandbox::worker {
ExecutionResult Worker::Init() noexcept {
  return js_engine_->Init();
}

ExecutionResult Worker::Run() noexcept {
  return js_engine_->Run();
}

ExecutionResult Worker::Stop() noexcept {
  return js_engine_->Stop();
}

ExecutionResultOr<js_engine::ExecutionResponse> Worker::RunCode(
    const string& code, const vector<string_view>& input,
    const flat_hash_map<string, string>& metadata,
    const Span<const uint8_t>& wasm) {
  auto request_type_or =
      WorkerUtils::GetValueFromMetadata(metadata, kRequestType);
  RETURN_IF_FAILURE(request_type_or.result());

  ROMA_VLOG(2) << "Worker executing request with code type of "
               << *request_type_or;

  auto code_version_or =
      WorkerUtils::GetValueFromMetadata(metadata, kCodeVersion);
  RETURN_IF_FAILURE(code_version_or.result());

  ROMA_VLOG(2) << "Worker executing request with code version number of "
               << *code_version_or;

  auto action_or = WorkerUtils::GetValueFromMetadata(metadata, kRequestAction);
  RETURN_IF_FAILURE(action_or.result());

  ROMA_VLOG(2) << "Worker executing request with action of " << *action_or;

  string handler_name = "";
  auto handler_name_or =
      WorkerUtils::GetValueFromMetadata(metadata, kHandlerName);
  // If we read the handler name successfully, let's store it.
  // Else if we didn't read it and the request is not a load request,
  // then return the failure.
  if (handler_name_or.result().Successful()) {
    handler_name = *handler_name_or;
  } else if (*action_or != kRequestActionLoad) {
    return handler_name_or.result();
  }

  ROMA_VLOG(2) << "Worker executing request with handler name " << handler_name;

  RomaJsEngineCompilationContext context;
  // Only reuse the context if this is not a load request.
  // A load request for an existing version should overwrite the given version.
  if (*action_or != kRequestActionLoad &&
      compilation_contexts_.Contains(*code_version_or)) {
    context = compilation_contexts_.Get(*code_version_or);
  } else if (require_preload_ && *action_or != kRequestActionLoad) {
    // If we require preloads and we couldn't find a context and this is not a
    // load request, then bail out. This is an execution without a previous
    // load.
    return FailureExecutionResult(
        SC_ROMA_WORKER_MISSING_CONTEXT_WHEN_EXECUTING);
  }

  core::ExecutionResultOr<JsEngineExecutionResponse> response_or;
  if (*request_type_or == kRequestTypeJavascript) {
    ROMA_VLOG(2) << "Worker executing request as JavaScript object";

    response_or = js_engine_->CompileAndRunJs(code, handler_name, input,
                                              metadata, context);
    RETURN_IF_FAILURE(response_or.result());
  } else if (*request_type_or == kRequestTypeWasm) {
    ROMA_VLOG(2) << "Worker executing request as WASM object";

    response_or = js_engine_->CompileAndRunWasm(code, handler_name, input,
                                                metadata, context);
    RETURN_IF_FAILURE(response_or.result());
  } else if (*request_type_or == kRequestTypeJavascriptWithWasm) {
    ROMA_VLOG(2) << "Worker executing request as JavaScript with WASM object";

    response_or = js_engine_->CompileAndRunJsWithWasm(code, wasm, handler_name,
                                                      input, metadata, context);
    RETURN_IF_FAILURE(response_or.result());
  } else {
    return FailureExecutionResult(SC_ROMA_WORKER_REQUEST_TYPE_NOT_SUPPORTED);
  }

  if (*action_or == kRequestActionLoad &&
      response_or->compilation_context.has_context) {
    compilation_contexts_.Set(*code_version_or,
                              response_or->compilation_context);
    ROMA_VLOG(1) << "caching compilation context for version "
                 << *code_version_or;
  }
  return response_or->execution_response;
}
}  // namespace google::scp::roma::sandbox::worker
