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

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "roma/config/src/config.h"

namespace google::scp::roma {
enum class [[deprecated(
    "Going forward, this value will be ignored and the only supported return "
    "type will be string.")]] WasmDataType{kUnknownType, kUint32, kString,
                                           kListOfString};
/// @brief The key of timeout tag for request.
static constexpr char kTimeoutMsTag[] = "TimeoutMs";
/// @brief Default value for request execution timeout. If no timeout tag is
/// set, the default value will be used.
static constexpr int kDefaultExecutionTimeoutMs = 5000;
/// @brief The wasm code array name tag for request.
static constexpr char kWasmCodeArrayName[] = "roma.request.wasm_array_name";

// The code object containing untrusted code to be loaded into the Worker.
struct CodeObject {
  // The id of the code object.
  std::string id;
  // The version number of the code object.
  uint64_t version_num;
  // The javascript code to execute. If empty, this code object is wasm only.
  std::string js;
  // The wasm code to be executed in standalone mode.
  std::string wasm;
  // The wasm code array to be loaded and instantiated from the driver JS code.
  std::vector<std::uint8_t> wasm_bin;
  // Any key-value pair tags associated with this code object.
  absl::flat_hash_map<std::string, std::string> tags;
};

/**
 * @brief The invocation request containing handler name and inputs to invoke
 * with the pre-loaded untrusted code. Here, the input is a vector of string or
 * shared pointers to string.
 *
 * @tparam InputType the data type of input vector. Only can be std::string or
 * std::shared_ptr<std::string>.
 */
template <typename InputType>
struct InvocationRequest {
  static_assert(
      std::is_same<InputType, std::string>::value ||
          std::is_same<InputType, std::shared_ptr<std::string>>::value,
      "InputType must be type std::string or std::shared_ptr<std::string>");

  // The id of the invocation request.
  std::string id;
  // The version number of the untrusted code that performs the execution
  // object.
  uint64_t version_num{0};
  // The signature of the handler function to invoke.
  std::string handler_name;

  // The return type of the WASM handler. For wasm source code execution, this
  // field is required.
  [[deprecated(
      "Going forward, this value will be ignored and the only supported return "
      "type will be string.")]] WasmDataType wasm_return_type;
  // Any key-value pair tags associated with this code object.
  absl::flat_hash_map<std::string, std::string> tags;
  // The input arguments to invoke the handler function. The InputType string is
  // in a format that can be parsed as JSON.
  std::vector<InputType> input;
};

typedef InvocationRequest<std::string> InvocationRequestStrInput;
typedef InvocationRequest<std::shared_ptr<std::string>>
    InvocationRequestSharedInput;

// The response as result of execution of the code object or invocation request.
struct ResponseObject {
  // The id of the object.
  std::string id;
  // The response of the execution.
  std::string resp;
  // Execution metrics. Any key should be checked for existence.
  absl::flat_hash_map<std::string, int64_t> metrics;
};

typedef std::function<void(std::unique_ptr<absl::StatusOr<ResponseObject>>)>
    Callback;

// Async API.
// Execute single invocation request. Can only be called when a valid
// code object has been loaded.
absl::Status Execute(
    std::unique_ptr<InvocationRequestStrInput> invocation_request,
    Callback callback);

absl::Status Execute(
    std::unique_ptr<InvocationRequestSharedInput> invocation_request,
    Callback callback);

// Batch API
// void Callback(const vector<ResponseObject>&);
typedef std::function<void(const std::vector<absl::StatusOr<ResponseObject>>&)>
    BatchCallback;

// Async & Batch API.
// Batch execute a batch of invocation requests. Can only be called when a valid
// code object has been loaded.
absl::Status BatchExecute(std::vector<InvocationRequestStrInput>& batch,
                          BatchCallback batch_callback);

absl::Status BatchExecute(std::vector<InvocationRequestSharedInput>& batch,
                          BatchCallback batch_callback);

// Async API.
// Load code object to all Roma Workers.
absl::Status LoadCodeObj(std::unique_ptr<CodeObject> code_object,
                         Callback callback);

// Initialize Roma. This will internally call fork() to fork the workers.
absl::Status RomaInit(const Config& config = Config());

// Stop roma service, which will internally kill all workers and fail all
// outstanding requests at best effort.
absl::Status RomaStop();
}  // namespace google::scp::roma
