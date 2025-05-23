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

#include "cc/public/core/interface/execution_result.h"

namespace privacy_sandbox::pbs_common {
/**
 * @brief An interface to allow a class/component to be initialized
 */
class InitializableInterface {
 public:
  virtual ~InitializableInterface() = default;

  /**
   * @brief Responsible for initializing the class/component and any external
   * dependencies such as other services clients, and etc
   * @return ExecutionResult the result of the execution with possible error
   * code.
   */
  virtual ExecutionResult Init() noexcept = 0;
};
}  // namespace privacy_sandbox::pbs_common
