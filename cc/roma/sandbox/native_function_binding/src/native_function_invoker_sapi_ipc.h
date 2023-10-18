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

#include "cc/roma/interface/function_binding_io.pb.h"
#include "public/core/interface/execution_result.h"
#include "sandboxed_api/sandbox2/comms.h"

#include "error_codes.h"
#include "native_function_invoker.h"

namespace google::scp::roma::sandbox::native_function_binding {
/**
 * @brief Native function invoker that uses SAPI IPC to "call" a function by
 * sending data over a socket.
 *
 */
class NativeFunctionInvokerSapiIpc : public NativeFunctionInvoker {
 public:
  explicit NativeFunctionInvokerSapiIpc(int comms_fd);

  core::ExecutionResult Invoke(const std::string& function_name,
                               google::scp::roma::proto::FunctionBindingIoProto&
                                   function_binding_proto) noexcept override;

 private:
  std::unique_ptr<sandbox2::Comms> ipc_comms_;
};
}  // namespace google::scp::roma::sandbox::native_function_binding
