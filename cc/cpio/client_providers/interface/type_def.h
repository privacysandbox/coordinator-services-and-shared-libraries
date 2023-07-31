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
#include <utility>

#include "core/interface/async_context.h"
#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Callback to pack response in Any proto.
 *
 * @param context async context.
 * @return core::ExecutionResult execution result.
 */
template <typename TRequest, typename TResponse>
core::ExecutionResult CallbackToPackAnyResponse(
    core::AsyncContext<google::protobuf::Any, google::protobuf::Any>&
        any_context,
    core::AsyncContext<TRequest, TResponse>& context) {
  auto any_response = std::make_shared<google::protobuf::Any>();
  if (core::ExecutionResult(context.result) == core::SuccessExecutionResult()) {
    any_response->PackFrom(*context.response);
  }
  any_context.response = std::move(any_response);
  any_context.result = context.result;
  any_context.Finish();
  return core::SuccessExecutionResult();
}
}  // namespace google::scp::cpio::client_providers
