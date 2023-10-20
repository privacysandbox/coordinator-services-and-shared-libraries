
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
#include "core/tcp_traffic_forwarder/src/tcp_traffic_forwarder_socat.h"

#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <string>
#include <thread>

#include "core/common/global_logger/src/global_logger.h"
#include "core/common/uuid/src/uuid.h"
#include "core/tcp_traffic_forwarder/src/error_codes.h"

using google::scp::core::common::kZeroUuid;
using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::to_string;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

static constexpr char kTCPTrafficForwarderSocat[] = "TCPTrafficForwarderSocat";

namespace google::scp::core {
TCPTrafficForwarderSocat::TCPTrafficForwarderSocat(
    const string& local_port, const string& forwarding_address)
    : local_port_(local_port), forwarding_address_(forwarding_address) {}

static void SigChildHandler(int sig) {
  pid_t pid;
  int status;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {}
}

ExecutionResult TCPTrafficForwarderSocat::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult TCPTrafficForwarderSocat::Run() noexcept {
  // Preemptively kill any stale socat processes
  system("pkill -f socat");

  if (forwarding_address_.empty()) {
    SCP_INFO(kTCPTrafficForwarderSocat, kZeroUuid,
             "Forwarding address is empty, not starting socat just yet");
    return SuccessExecutionResult();
  }

  char socat_command_name[] = "socat";
  string arg1 = "-d";
  string arg2 = "-d";
  string arg3 = "-ly";
  string arg4 = "-lh";
  string arg5 = string("TCP-LISTEN:") + local_port_ + string(",fork,reuseaddr");
  string arg6 = "TCP:" + forwarding_address_;
  char* const command_arguments[8] = {socat_command_name,
                                      const_cast<char*>(arg1.c_str()),
                                      const_cast<char*>(arg2.c_str()),
                                      const_cast<char*>(arg3.c_str()),
                                      const_cast<char*>(arg4.c_str()),
                                      const_cast<char*>(arg5.c_str()),
                                      const_cast<char*>(arg6.c_str()),
                                      NULL};
  SCP_INFO(kTCPTrafficForwarderSocat, kZeroUuid,
           "Executing command: '%s %s %s %s %s %s %s'", socat_command_name,
           arg1.c_str(), arg2.c_str(), arg3.c_str(), arg4.c_str(), arg5.c_str(),
           arg6.c_str());

  // Fork an intermediate process that can wait pid other children
  int child_pid = fork();
  if (child_pid == -1) {
    SCP_ERROR(kTCPTrafficForwarderSocat, kZeroUuid,
              FailureExecutionResult(SC_UNKNOWN),
              "Failed to start child process. Error: %d", errno);
    return FailureExecutionResult(
        errors::SC_TCP_TRAFFIC_FORWARDER_CANNOT_START_DUE_TO_FORK_ERROR);
  }

  if (child_pid == 0) {
    // Register handler to properly terminate socat processes that die.
    struct sigaction sigchild_action {};

    sigchild_action.sa_handler = SigChildHandler;
    sigaction(SIGCHLD, &sigchild_action, nullptr);

    // Close all open file descriptors starting after stdin (0), stdout (1),
    // stderr (2). This ensures discarding any sockets that PBS has opened
    // before fork.
    int64_t fdlimit = sysconf(_SC_OPEN_MAX);
    for (int64_t i = STDERR_FILENO + 1; i < fdlimit; i++) {
      close(i);
    }

    // Give a new process group and session to the child process.
    // Set the process group to child's PID to detach from PBS's process group.
    // This ensures that if socat gets killed at process group level, PBS is not
    // affected by it.
    int error = setsid();
    if (error == -1) {
      cerr << "Child: Failed to set PGID and SID. errno: " << errno << endl;
      exit(1);
    } else {
      cout << "Child: Set the process PGID and SID to " << getpgid(getpid())
           << ", " << getsid(getpid()) << " respectively" << endl;
    }

#if !defined(_SCP_CORE_SOCAT_FORWARDER_NON_PRIVILEGED)
    static constexpr int kNobodyUserId = 65534;
    error = setgid(kNobodyUserId);
    if (error == -1) {
      cerr << "Child: Failed to set process group ID. errno: " << errno << endl;
      exit(1);
    } else {
      cout << "Child: Set the process real group ID to: " << getgid()
           << " effective group ID: " << getegid() << endl;
    }

    error = setuid(kNobodyUserId);
    if (error == -1) {
      cerr << "Child: Failed to set process user ID. errno: " << errno << endl;
      exit(1);
    } else {
      cout << "Child: Set the process real user ID to: " << getuid()
           << " effective user ID: " << geteuid() << endl;
    }
#endif

    auto socat_pid = fork();

    if (socat_pid == -1) {
      SCP_ERROR(kTCPTrafficForwarderSocat, kZeroUuid,
                FailureExecutionResult(SC_UNKNOWN),
                "Failed to start socat process. Error: %d", errno);
      exit(1);
    }

    if (socat_pid == 0) {
      error = execvp(socat_command_name, command_arguments);
      if (error == -1) {
        // Exec failure, exit the child process
        SCP_ERROR(kTCPTrafficForwarderSocat, kZeroUuid,
                  FailureExecutionResult(SC_UNKNOWN),
                  "Failed to exec socat process. Error: %d", errno);
        exit(1);
      }
    } else {
      waitpid(socat_pid, NULL, 0);
    }
  } else {
    SCP_INFO(kTCPTrafficForwarderSocat, kZeroUuid,
             "Child process started with pid: %d", child_pid);
    child_pid_ = child_pid;
  }

  return SuccessExecutionResult();
}

ExecutionResult TCPTrafficForwarderSocat::Stop() noexcept {
  SCP_INFO(kTCPTrafficForwarderSocat, kZeroUuid, "About to stop socat.");
  auto child_pid = child_pid_.load();
  if (child_pid == -1) {
    SCP_INFO(kTCPTrafficForwarderSocat, kZeroUuid,
             "Socat process not started. So nothing to stop.");
    return SuccessExecutionResult();
  }

  // Terminate the process group with -PID. (-) indicates terminate the group
  // instead of process with the PID.
  int error = kill(-child_pid, SIGTERM);
  if (error == -1) {
    SCP_ERROR(kTCPTrafficForwarderSocat, kZeroUuid,
              FailureExecutionResult(SC_UNKNOWN),
              "Could not kill the socat processes. errno: %d", errno);
  } else {
    SCP_INFO(kTCPTrafficForwarderSocat, kZeroUuid,
             "Successfully killed socat processes with pid %d", child_pid);
    child_pid_ = -1;
  }

  // Give the socat processes some time do die.
  sleep_for(milliseconds(100));
  // Account for any potentially orphaned socat processes that should be killed.
  auto there_are_socat_processes_running = []() -> bool {
    char line_buffer[256];
    std::string result = "";
    // Check whether any process with command "socat" is running, and exclude
    // the grep search form the results.
    FILE* pipe = popen("ps xao comm | grep 'socat' | grep -v 'grep'", "r");
    if (!pipe) {
      // True so that we retry
      return true;
    }
    try {
      while (fgets(line_buffer, sizeof(line_buffer), pipe) != NULL) {
        result += line_buffer;
        // One line is enough, if there's a socat process, we'll get it.
        break;
      }
    } catch (...) {
      pclose(pipe);
      // True so that we retry
      return true;
    }
    pclose(pipe);
    return result.find("socat") != result.npos;
  };

  auto kill_all_socat_processes =
      [&there_are_socat_processes_running]() -> bool {
    auto pipe = popen("pkill -f socat", "r");
    if (pipe == NULL) {
      return false;
    }
    pclose(pipe);

    return !there_are_socat_processes_running();
  };

  while (!kill_all_socat_processes()) {
    SCP_INFO(kTCPTrafficForwarderSocat, kZeroUuid,
             "Waiting for all socat processes to die.");
    sleep_for(milliseconds(100));
  }

  child_pid_ = -1;
  return SuccessExecutionResult();
}

ExecutionResult TCPTrafficForwarderSocat::ResetForwardingAddress(
    const string& forwarding_address) noexcept {
  if (forwarding_address == forwarding_address_) {
    SCP_INFO(kTCPTrafficForwarderSocat, kZeroUuid,
             "Forwarding address is same as before: %s. Not resetting.",
             forwarding_address.c_str());
    return SuccessExecutionResult();
  }
  SCP_INFO(kTCPTrafficForwarderSocat, kZeroUuid,
           "Forwarding address changed. Restarting Traffic Forwarder "
           "with address: %s",
           forwarding_address.c_str());
  auto execution_result = Stop();
  if (!execution_result.Successful()) {
    return execution_result;
  }
  forwarding_address_ = forwarding_address;
  return Run();
}
}  // namespace google::scp::core
