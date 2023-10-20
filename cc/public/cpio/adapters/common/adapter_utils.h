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
#include <utility>

#include "core/interface/async_context.h"
#include "core/utils/src/error_utils.h"
#include "public/core/interface/execution_result.h"

static constexpr char kCpioClient[] = "CpioClient";

namespace google::scp::cpio {
/**
 * @brief Executes the async call.
 *
 * @tparam TRequest request type of the async call.
 * @tparam TResponse response type of the async call.
 * @param context async context.
 * @return core::ExecutionResult execution result.
 */
template <typename TRequest, typename TResponse>
void OnExecutionCallback(Callback<TResponse>& client_callback,
                         core::AsyncContext<TRequest, TResponse>& context) {
  if (!context.result.Successful()) {
    SCP_ERROR_CONTEXT(kCpioClient, context, context.result,
                      "Failed to execute.");
    client_callback(core::utils::ConvertToPublicExecutionResult(context.result),
                    TResponse());
    return;
  }
  client_callback(core::utils::ConvertToPublicExecutionResult(context.result),
                  std::move(*context.response));
}

/**
 * @brief Executes the async call.
 *
 * @tparam TRequest request type of the async call.
 * @tparam TResponse response type of the async call.
 * @param context async context.
 * @return core::ExecutionResult execution result.
 */
template <typename TRequest, typename TResponse>
core::ExecutionResult Execute(std::function<core::ExecutionResult(
                                  core::AsyncContext<TRequest, TResponse>&)>
                                  func,
                              TRequest request, Callback<TResponse> callback) {
  auto activity_id = core::common::Uuid::GenerateUuid();
  core::AsyncContext<TRequest, TResponse> context(
      std::make_shared<TRequest>(std::move(request)),
      bind(OnExecutionCallback<TRequest, TResponse>, callback,
           std::placeholders::_1),
      activity_id, activity_id);

  return core::utils::ConvertToPublicExecutionResult(func(context));
}
}  // namespace google::scp::cpio
