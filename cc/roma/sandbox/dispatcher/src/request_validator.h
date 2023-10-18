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

#include "public/core/interface/execution_result.h"
#include "roma/interface/roma.h"
#include "roma/sandbox/constants/constants.h"
#include "roma/sandbox/worker_api/src/worker_api.h"

namespace google::scp::roma::sandbox::dispatcher::request_validator {
template <typename T>
struct RequestValidator {};

/**
 * @brief Template specialization to validate a CodeObject.
 */
template <>
struct RequestValidator<CodeObject> {
  static core::ExecutionResult Validate(
      const std::unique_ptr<CodeObject>& request) {
    if (!request) {
      return core::FailureExecutionResult(SC_UNKNOWN);
    }

    if (request->js.empty() && request->wasm.empty()) {
      return core::FailureExecutionResult(SC_UNKNOWN);
    }

    if (!request->js.empty() && !request->wasm.empty()) {
      return core::FailureExecutionResult(SC_UNKNOWN);
    }

    if (request->version_num == 0) {
      return core::FailureExecutionResult(SC_UNKNOWN);
    }

    if (request->id.empty()) {
      return core::FailureExecutionResult(SC_UNKNOWN);
    }

    return core::SuccessExecutionResult();
  }
};

/**
 * @brief Common validation fields for invocation requests.
 */
template <typename RequestT>
static core::ExecutionResult InvocationRequestCommon(const RequestT& request) {
  if (!request) {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  if (request->handler_name.empty()) {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  if (request->version_num == 0) {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  if (request->id.empty()) {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }

  return core::SuccessExecutionResult();
}

/**
 * @brief Template specialization to validate a InvocationRequestStrInput.
 */
template <>
struct RequestValidator<InvocationRequestStrInput> {
  static core::ExecutionResult Validate(
      const std::unique_ptr<InvocationRequestStrInput>& request) {
    return InvocationRequestCommon(request);
  }
};

/**
 * @brief Template specialization to validate a InvocationRequestSharedInput.
 */
template <>
struct RequestValidator<InvocationRequestSharedInput> {
  static core::ExecutionResult Validate(
      const std::unique_ptr<InvocationRequestSharedInput>& request) {
    return InvocationRequestCommon(request);
  }
};
}  // namespace google::scp::roma::sandbox::dispatcher::request_validator
