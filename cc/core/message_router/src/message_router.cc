// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "message_router.h"

#include <memory>

#include "google/protobuf/any.pb.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

using google::protobuf::Any;

namespace google::scp::core {

ExecutionResult MessageRouter::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult MessageRouter::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult MessageRouter::Stop() noexcept {
  return SuccessExecutionResult();
}

void MessageRouter::OnMessageReceived(
    const std::shared_ptr<AsyncContext<Any, Any>>& context) noexcept {
  AsyncAction action;
  auto result = actions_.Find(context->request->type_url(), action);
  if (!result) {
    auto failure_result = FailureExecutionResult(
        errors::SC_MESSAGE_ROUTER_REQUEST_NOT_SUBSCRIBED);
    context->result = failure_result;
    context->Finish();
  } else {
    action(*context);
  }
}

ExecutionResult MessageRouter::Subscribe(const std::string& request_type,
                                         const AsyncAction& action) noexcept {
  AsyncAction existing_action;
  auto result = actions_.Insert({request_type, action}, existing_action);
  if (!result) {
    return FailureExecutionResult(
        errors::SC_MESSAGE_ROUTER_REQUEST_ALREADY_SUBSCRIBED);
  }
  return result;
}
}  // namespace google::scp::core
