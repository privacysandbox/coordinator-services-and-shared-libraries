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

#include <stddef.h>

#include <memory>
#include <vector>

#include "function_binding_object.h"

namespace google::scp::roma {
class Config {
 public:
  size_t NumberOfWorkers = 0;
  size_t ThreadsPerWorker = 0;
  size_t IpcMemorySize = 0;
  size_t QueueMaxItems = 0;

  // The maximum number of pages that the WASM memory can use. Each page is
  // 64KiB. Will be clamped to 65536 (4GiB) if larger.
  // If left at zero, the default behavior is to use the maximum value allowed
  // (up to 4GiB).
  size_t MaxWasmMemoryNumberOfPages = 0;

  /**
   * @brief Register a function binding object
   *
   * @tparam TOutput
   * @tparam TInputs
   * @param function_binding
   */
  template <typename TOutput, typename... TInputs>
  void RegisterFunctionBinding(
      std::unique_ptr<FunctionBindingObject<TOutput, TInputs...>>
          function_binding) {
    function_bindings_.emplace_back(function_binding.get());
    function_binding.release();
  }

  /**
   * @brief Get a copy of the registered function binding objects
   *
   * @param[out] function_bindings
   */
  void GetFunctionBindings(
      std::vector<std::shared_ptr<FunctionBindingObjectBase>>&
          function_bindings) const {
    function_bindings = std::vector<std::shared_ptr<FunctionBindingObjectBase>>(
        function_bindings_.begin(), function_bindings_.end());
  }

 private:
  /**
   * @brief User-registered function JS/C++ function bindings
   */
  std::vector<std::shared_ptr<FunctionBindingObjectBase>> function_bindings_;
};
}  // namespace google::scp::roma
