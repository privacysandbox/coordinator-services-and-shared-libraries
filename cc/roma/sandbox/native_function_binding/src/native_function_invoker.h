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

#include <string>

#include "cc/roma/interface/function_binding_io.pb.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::roma::sandbox::native_function_binding {
/**
 * @brief Interface for a native function invoker, used to bridge between a
 * JS engine an a native C++ function. Generally, a C++ function would be
 * registered that can be called from a JS engine implementation, when a JS
 * function is called. This interface should be used to wrap specific
 * implementations. This is the handler of the call on the JS engine side.
 *
 */
class NativeFunctionInvoker {
 public:
  /**
   * @brief Invoke a native function linked to the given function invocation.
   * @param function_name is the name of the function to invoke. This is the
   * name that was used to register the function on the JS side.
   * @param[inout] function_binding_proto is the function binding context. It
   * contains both the input to be passed to the c++ function as well as the
   * return value that is set by the c++ function and which is passed to the JS
   * function as a return. This is a two-way proto.
   */
  virtual core::ExecutionResult Invoke(
      const std::string& function_name,
      google::scp::roma::proto::FunctionBindingIoProto&
          function_binding_proto) noexcept = 0;
};
}  // namespace google::scp::roma::sandbox::native_function_binding
