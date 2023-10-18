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

#include <functional>
#include <string>

#include "cc/roma/interface/function_binding_io.pb.h"

namespace google::scp::roma {
class FunctionBindingObjectV2 {
 public:
  /**
   * @brief The name by which Javascript code can call this function.
   */
  std::string function_name;

  /**
   * @brief The function that will be bound to a Javascript function.
   */
  std::function<void(proto::FunctionBindingIoProto&)> function;
};
}  // namespace google::scp::roma
