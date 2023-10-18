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

#include "daemonizer.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>

#include "error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;

using google::scp::core::errors::DAEMONIZER_FAILED_PARSING_INPUT;
using google::scp::core::errors::
    DAEMONIZER_FAILED_WAITING_FOR_LAUNCHED_PROCESSES;
using google::scp::core::errors::DAEMONIZER_INVALID_INPUT;
using google::scp::core::errors::DAEMONIZER_UNKNOWN_ERROR;

namespace google::scp::process_launcher {
ExecutionResult Daemonizer::Run() noexcept {
  if (executable_count_ < 1) {
    return FailureExecutionResult(DAEMONIZER_INVALID_INPUT);
  }

  auto result = GetExecutableArgs();
  if (!result.Successful()) {
    std::cerr << "Failed parsing input arguments with: "
              << GetErrorMessage(result.status_code) << std::endl;

    return FailureExecutionResult(DAEMONIZER_FAILED_PARSING_INPUT);
  }

  executable_arg_to_launch_set_ =
      std::unordered_set<std::shared_ptr<ExecutableArgument>>(
          executable_args_.begin(), executable_args_.end());

  while (true) {
    for (auto& exe_arg : executable_args_) {
      if (executable_arg_to_launch_set_.find(exe_arg) ==
          executable_arg_to_launch_set_.end()) {
        // This process does not need to be launched
        continue;
      }

      std::vector<char*> cstring_vec;
      exe_arg->ToExecutableVector(cstring_vec);
      char** args = cstring_vec.data();

      pid_t proc_pid = -1;
      if ((proc_pid = fork()) == 0) {
        // Child process context
        // Execute the input process
        execvp(exe_arg->executable_name.c_str(), args);

        std::cerr << "Failed to start process ["
                  << exe_arg->executable_name + "] with error ["
                  << std::strerror(errno) << "]" << std::endl;
        exit(-1);
      }

      std::cout << "Started process [" << exe_arg->executable_name
                << "] with pid [" << proc_pid << "]" << std::endl;

      pid_to_executable_arg_map_[proc_pid] = exe_arg;
      // Since we launched this process, we remove it from the set
      // of processes to start.
      executable_arg_to_launch_set_.erase(exe_arg);
    }

    // Wait for any launched process to exit.
    // This call blocks until a child process exits.
    int exit_status;
    pid_t failed_proc_pid = wait(&exit_status);

    if (failed_proc_pid == -1) {
      return FailureExecutionResult(
          DAEMONIZER_FAILED_WAITING_FOR_LAUNCHED_PROCESSES);
    }

    if (WIFEXITED(exit_status)) {
      std::cout << "Child process EXITED with status : "
                << WEXITSTATUS(exit_status) << std::endl;
    } else if (WIFSIGNALED(exit_status)) {
      std::cout << "Child process was SIGNALLED with signal number: "
                << WTERMSIG(exit_status) << std::endl;
    } else {
      std::cerr << "Child process's action is UNKNOWN" << std::endl;
    }

    if (ShouldStopRestartingProcesses()) {
      break;
    }

    // If this is an unknown PID, just continue. This is most likely a process
    // started by a child, which was then orphaned and ended up parented by this
    // process.
    if (pid_to_executable_arg_map_.find(failed_proc_pid) ==
        pid_to_executable_arg_map_.end()) {
      std::cout << "A child process which was not explicitly started has died. "
                   "This is most likely a grandchild process."
                << std::endl;
      continue;
    }

    auto failed_process_arg = pid_to_executable_arg_map_[failed_proc_pid];
    // This PID is no longer valid so we remove the mapping from PID to
    // executable arg
    pid_to_executable_arg_map_.erase(failed_proc_pid);
    if (failed_process_arg->restart) {
      // Because this process exited we need to launch it again, so we add its
      // executable arg to the set of processes to launch.
      executable_arg_to_launch_set_.insert(failed_process_arg);

      std::cout << "Process with executable_name ["
                << failed_process_arg->executable_name
                << "] exited. Restarting it..." << std::endl;
    } else {
      std::cout << "Process with executable_name ["
                << failed_process_arg->executable_name
                << "] exited. Will NOT restart it..." << std::endl;
    }

    // TODO: change to exponential backoff with absolute failure if
    // too many failures within a time interval
    usleep(500 * 1000);
  }  // while(true)

  return FailureExecutionResult(DAEMONIZER_UNKNOWN_ERROR);
}

ExecutionResult Daemonizer::GetExecutableArgs() noexcept {
  for (int i = 0; i < executable_count_; i++) {
    ExecutableArgument parsed_arg;
    auto result =
        executable_arg_parser_.Parse(std::string(executables_[i]), parsed_arg);

    if (!result.Successful()) {
      return result;
    }

    executable_args_.push_back(
        std::make_shared<ExecutableArgument>(parsed_arg));
  }

  return SuccessExecutionResult();
}

bool Daemonizer::ShouldStopRestartingProcesses() noexcept {
  return false;
}
}  // namespace google::scp::process_launcher
