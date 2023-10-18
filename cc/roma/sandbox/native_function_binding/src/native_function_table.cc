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

#include "native_function_table.h"

#include "error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_ROMA_FUNCTION_TABLE_COULD_NOT_FIND_FUNCTION_NAME;
using google::scp::core::errors::SC_ROMA_FUNCTION_TABLE_NAME_ALREADY_REGISTERED;
using std::lock_guard;
using std::string;

namespace google::scp::roma::sandbox::native_function_binding {
ExecutionResult NativeFunctionTable::Register(string function_name,
                                              NativeBinding binding) {
  lock_guard lock(native_functions_map_mutex_);

  if (native_functions_.find(function_name) != native_functions_.end()) {
    return FailureExecutionResult(
        SC_ROMA_FUNCTION_TABLE_NAME_ALREADY_REGISTERED);
  }

  native_functions_[function_name] = binding;

  return SuccessExecutionResult();
}

ExecutionResult NativeFunctionTable::Call(
    string function_name,
    proto::FunctionBindingIoProto& function_binding_proto) {
  NativeBinding func;

  {
    lock_guard lock(native_functions_map_mutex_);
    if (native_functions_.find(function_name) == native_functions_.end()) {
      return FailureExecutionResult(
          SC_ROMA_FUNCTION_TABLE_COULD_NOT_FIND_FUNCTION_NAME);
    }
    func = native_functions_[function_name];
  }

  func(function_binding_proto);

  return SuccessExecutionResult();
}
}  // namespace google::scp::roma::sandbox::native_function_binding
