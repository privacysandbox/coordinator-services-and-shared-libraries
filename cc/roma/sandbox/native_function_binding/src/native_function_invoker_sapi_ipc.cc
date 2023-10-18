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

#include "native_function_invoker_sapi_ipc.h"

#include "roma/sandbox/constants/constants.h"

#include "error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_ROMA_FUNCTION_INVOKER_SAPI_IPC_COULD_NOT_RECV_RESPONSE_FROM_PARENT;
using google::scp ::core::errors ::
    SC_ROMA_FUNCTION_INVOKER_SAPI_IPC_COULD_NOT_SEND_CALL_TO_PARENT;
using google::scp::core::errors::
    SC_ROMA_FUNCTION_INVOKER_SAPI_IPC_INVOKE_WITH_UNINITIALIZED_COMMS;
using google::scp::roma::proto::FunctionBindingIoProto;
using google::scp::roma::sandbox::constants::
    kFuctionBindingMetadataFunctionName;
using std::make_unique;
using std::string;

static constexpr int kBadFd = -1;

namespace google::scp::roma::sandbox::native_function_binding {
NativeFunctionInvokerSapiIpc::NativeFunctionInvokerSapiIpc(int comms_fd) {
  if (comms_fd != kBadFd) {
    ipc_comms_ = make_unique<sandbox2::Comms>(comms_fd);
  }
}

ExecutionResult NativeFunctionInvokerSapiIpc::Invoke(
    const string& function_name,
    FunctionBindingIoProto& function_binding_proto) noexcept {
  if (!ipc_comms_) {
    return FailureExecutionResult(
        SC_ROMA_FUNCTION_INVOKER_SAPI_IPC_INVOKE_WITH_UNINITIALIZED_COMMS);
  }

  // Set the function name so that it can be retrieved on the other side.
  (*function_binding_proto
        .mutable_metadata())[kFuctionBindingMetadataFunctionName] =
      function_name;

  auto sent = ipc_comms_->SendProtoBuf(function_binding_proto);
  if (!sent) {
    return FailureExecutionResult(
        SC_ROMA_FUNCTION_INVOKER_SAPI_IPC_COULD_NOT_SEND_CALL_TO_PARENT);
  }

  auto recv = ipc_comms_->RecvProtoBuf(&function_binding_proto);
  if (!recv) {
    return FailureExecutionResult(
        SC_ROMA_FUNCTION_INVOKER_SAPI_IPC_COULD_NOT_RECV_RESPONSE_FROM_PARENT);
  }

  return SuccessExecutionResult();
}
}  // namespace google::scp::roma::sandbox::native_function_binding
