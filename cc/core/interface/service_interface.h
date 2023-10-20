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

#include "public/core/interface/execution_result.h"

namespace google::scp::core {
/**
 * @brief Any top level component in the core library must inherit this
 * interface and override the Init, Run, and Stop functionalities.
 */
class ServiceInterface {
 public:
  virtual ~ServiceInterface() = default;

  /**
   * @brief Responsible for initializing the main component and any external
   * dependencies such as other services clients, and etc
   * @return ExecutionResult the result of the execution with possible error
   * code.
   */
  virtual ExecutionResult Init() noexcept = 0;

  /**
   * @brief Think about Run as the first step after all the components in the
   * system have initialized. In this step you can start internal functionality
   * for the component. Anything like starting an internal thread pool, garbage
   * collection, and etc.
   * @return ExecutionResult the result of the execution with possible error
   * code.
   */
  virtual ExecutionResult Run() noexcept = 0;

  /**
   * @brief To cleanly exit a process, we need to ensure that all the components
   * have stopped first. So please make sure that all of the internals have been
   * stopped after this function.
   * @return ExecutionResult the result of the execution with possible error
   * code.
   */
  virtual ExecutionResult Stop() noexcept = 0;
};
}  // namespace google::scp::core
