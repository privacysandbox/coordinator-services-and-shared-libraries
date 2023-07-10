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

#include <sys/wait.h>
#include <unistd.h>

#include "public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::roma::common {

/**
 * @brief Process work request in SharedMemory.
 */
class Process {
 public:
  /**
   * @brief Creates a process to execute the work.
   *
   * @tparam Callable the INVOKE operation may be performed.
   * @param work the work to be executed.
   * @param process_pid the pid of the process for work operation.
   * @return core::ExecutionResult
   */
  template <typename Callable>
  static core::ExecutionResult Create(const Callable& work,
                                      pid_t& process_pid) noexcept {
    process_pid = fork();
    if (process_pid == -1) {
      return core::FailureExecutionResult(
          core::errors::SC_ROMA_PROCESS_CREATE_FAILURE);
    }

    if (process_pid == 0) {
      core::ExecutionResult execution_result = work();
      if (execution_result != core::SuccessExecutionResult()) {
        exit(1);
      }
      exit(0);
    }
    return core::SuccessExecutionResult();
  }
};
}  // namespace google::scp::roma::common
