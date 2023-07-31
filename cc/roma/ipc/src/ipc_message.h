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

#include <memory>
#include <string>
#include <utility>

#include "public/core/interface/execution_result.h"
#include "roma/common/src/containers.h"
#include "roma/common/src/shm_allocator.h"
#include "roma/interface/roma.h"

#include "error_codes.h"

namespace google::scp::roma::ipc {
using common::RomaMap;
using common::RomaString;
using common::RomaVector;
using common::ShmAllocated;

/// The code object allocated on shared memory
struct RomaCodeObj : public ShmAllocated {
  RomaString id;
  uint64_t version_num{0};
  RomaString js;
  RomaString wasm;
  WasmDataType wasm_return_type;
  RomaString handler_name;
  RomaVector<RomaString> input;
  RomaMap<RomaString, RomaString> tags;

  explicit RomaCodeObj(const CodeObject& obj)
      : id(obj.id), version_num(obj.version_num), js(obj.js), wasm(obj.wasm) {
    tags.insert(obj.tags.begin(), obj.tags.end());
  }

  explicit RomaCodeObj(const InvocationRequestStrInput& obj)
      : id(obj.id),
        version_num(obj.version_num),
        wasm_return_type(obj.wasm_return_type),
        handler_name(obj.handler_name),
        input(obj.input.begin(), obj.input.end()) {
    tags.insert(obj.tags.begin(), obj.tags.end());
  }

  explicit RomaCodeObj(const InvocationRequestSharedInput& obj)
      : id(obj.id),
        version_num(obj.version_num),
        wasm_return_type(obj.wasm_return_type),
        handler_name(obj.handler_name) {
    tags.insert(obj.tags.begin(), obj.tags.end());
    for (auto& item : obj.input) {
      input.emplace_back(*item);
    }
  }

  // Copy constructor
  RomaCodeObj(const RomaCodeObj& obj) {
    id = obj.id;
    version_num = obj.version_num;
    js = obj.js;
    wasm = obj.wasm;
    wasm_return_type = obj.wasm_return_type;
    handler_name = obj.handler_name;
    tags.insert(obj.tags.begin(), obj.tags.end());
    for (auto& item : obj.input) {
      input.emplace_back(item);
    }
  }

  /// @brief  Whether the object has JavaScript or WASM fields defined.
  bool Empty() const { return js.empty() && wasm.empty(); }

  /// @brief Checks JavaScript code is empty or not. It will return true if
  /// JavaScript code isn't defined.
  bool JsIsEmpty() const { return js.empty(); }

  /// @brief Checks WASM code is empty or not. It will return true if
  /// only WASM code isn't defined.
  bool WasmIsEmpty() const { return wasm.empty(); }

  /**
   * @brief Gets the Request tag value.
   *
   * @param tag_name The tag name to query.
   * @param tag_value The tag value.
   * @return core::ExecutionResult
   */
  core::ExecutionResult GetCodeObjTag(
      const common::RomaString& tag_name,
      common::RomaString& tag_value) const noexcept {
    if (tags.empty()) {
      return core::FailureExecutionResult(
          core::errors::SC_ROMA_IPC_MESSAGE_REQUEST_TAG_NOT_FOUND);
    }

    auto it = tags.find(tag_name);
    if (it != tags.end()) {
      tag_value = it->second;
      return core::SuccessExecutionResult();
    }
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_IPC_MESSAGE_REQUEST_TAG_NOT_FOUND);
  }
};

enum class RequestType { kUpdate, kExecute };

/// The request sent from dispatcher to workers.
struct Request : public ShmAllocated {
  // The code object to execute
  std::unique_ptr<RomaCodeObj> code_obj;
  // The callback to call when the execution finishes. Note that this may only
  // be valid on the dispatcher side. It may not be accessible from workers, as
  // it may reference memory on the dispatcher process.
  std::unique_ptr<Callback> callback;
  RequestType type;

  // Used to determine whether this request has been worked on before.
  bool has_been_worked = false;

  Request() {}

  Request(Request&& other)
      : code_obj(std::move(other.code_obj)),
        callback(std::move(other.callback)),
        type(other.type),
        has_been_worked(other.has_been_worked) {}

  Request(std::unique_ptr<CodeObject> code_obj, Callback callback)
      : code_obj(new RomaCodeObj(*code_obj)),
        callback(std::make_unique<Callback>(callback)),
        type(RequestType::kUpdate) {}

  Request(std::unique_ptr<CodeObject> code_obj, Callback callback,
          RequestType type)
      : code_obj(new RomaCodeObj(*code_obj)),
        callback(std::make_unique<Callback>(callback)),
        type(type) {}

  Request(std::unique_ptr<InvocationRequestStrInput> invocation_req,
          Callback callback)
      : code_obj(new RomaCodeObj(*invocation_req)),
        callback(std::make_unique<Callback>(callback)),
        type(RequestType::kExecute) {}

  Request(std::unique_ptr<InvocationRequestStrInput> invocation_req,
          Callback callback, RequestType type)
      : code_obj(new RomaCodeObj(*invocation_req)),
        callback(std::make_unique<Callback>(callback)),
        type(type) {}

  Request(std::unique_ptr<InvocationRequestSharedInput> invocation_req,
          Callback callback)
      : code_obj(new RomaCodeObj(*invocation_req)),
        callback(std::make_unique<Callback>(callback)),
        type(RequestType::kExecute) {}

  Request(std::unique_ptr<InvocationRequestSharedInput> invocation_req,
          Callback callback, RequestType type)
      : code_obj(new RomaCodeObj(*invocation_req)),
        callback(std::make_unique<Callback>(callback)),
        type(type) {}
};

struct RomaCodeResponse : public ShmAllocated {
  RomaString id;
  RomaString resp;
};

enum class ResponseStatus { kUnknown = 0, kSucceeded, kFailed };

/// The response sent from workers to dispatcher
struct Response : public ShmAllocated {
  core::ExecutionResult result;
  std::unique_ptr<RomaCodeResponse> response;
  std::unique_ptr<Request> request;
  ResponseStatus status;

  Response() : status(ResponseStatus::kUnknown) {}

  ResponseObject CreateCodeResponse() const {
    return ResponseObject{std::string(response->id),
                          std::string(response->resp)};
  }
};

// A work item entry where the dispatcher places a request, and the worker a
// response
struct WorkItem : public ShmAllocated {
  std::unique_ptr<Request> request;
  std::unique_ptr<Response> response;

  WorkItem() : request(nullptr), response(nullptr) {}

  void Complete(std::unique_ptr<Response> resp) { response.swap(resp); }

  bool Succeeded() {
    return response && response->status == ResponseStatus::kSucceeded;
  }

  bool Failed() {
    return response && response->status == ResponseStatus::kFailed;
  }

  bool Completed() { return Succeeded() || Failed(); }
};
}  // namespace google::scp::roma::ipc
