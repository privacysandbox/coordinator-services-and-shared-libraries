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

#include "process_launcher/daemonizer/src/daemonizer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sys/prctl.h>
#include <unistd.h>

#include <string>

#include <nlohmann/json.hpp>

#include "core/common/uuid/src/uuid.h"
#include "core/test/scp_test_base.h"
#include "process_launcher/argument_parser/src/json_arg_parser.h"
#include "process_launcher/daemonizer/src/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::common::Uuid;
using google::scp::core::test::ScpTestBase;
using json = nlohmann::json;
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::DAEMONIZER_INVALID_INPUT;
using google::scp::core::test::ResultIs;
using google::scp::process_launcher::Daemonizer;
using google::scp::process_launcher::ExecutableArgument;

namespace google::scp::process_launcher::test {
constexpr int five_hundres_ms = 500000;

class DaemonizerForTests : public Daemonizer {
 public:
  DaemonizerForTests() = delete;

  DaemonizerForTests(int executable_count, char* executables[])
      : Daemonizer(executable_count, executables) {}

  void StopAfterThisManyRestarts(int this_many) {
    restarts_to_stop_after_ = this_many;
  }

  std::vector<std::shared_ptr<ExecutableArgument>> GetExecutableArguments() {
    GetExecutableArgs();
    return executable_args_;
  }

 protected:
  bool ShouldStopRestartingProcesses() noexcept override {
    return restarts_to_stop_after_-- == 0;
  }

 private:
  int restarts_to_stop_after_ = INT32_MAX;
};

static std::string GetRandomProcessName() {
  auto uuid = Uuid::GenerateUuid();
  return std::to_string(uuid.low) + std::to_string(uuid.low) + "_test_process";
}

/**
 * @brief Get command to create a dummy named process which just sleeps for 10
 * minutes and quits
 *
 * @param process_name the name of the process
 * @return std::string the command
 */
static std::string GetCommandToStartNamedProcess(std::string process_name,
                                                 bool restart = true) {
  // Start a named process:
  // bash -c "exec -a <name> <cmd_to_execute>"
  json json_cmd;
  json_cmd["executable_name"] = "bash";
  auto cmd = "exec -a " + process_name + " sleep 600";
  json_cmd["command_line_args"] = std::vector<std::string>{"-c", cmd};
  if (!restart) {
    json_cmd["restart"] = false;
  }
  return json_cmd.dump();
}

static std::string RunCommand(std::string& cmd) {
  char buffer[128];
  memset(buffer, 0, sizeof(buffer));
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                pclose);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  fread(buffer, sizeof(buffer) - 1, 1, pipe.get());
  return std::string(buffer);
}

static pid_t GetProcessId(std::string& process_name) {
  auto get_pid_cmd = "pidof " + process_name;
  std::string pid_str = RunCommand(get_pid_cmd);
  return std::stoi(pid_str.empty() ? "-1" : pid_str);
}

static bool ProcessExists(std::string process_name) {
  return GetProcessId(process_name) != -1;
}

static void WaitForProcessToExist(std::string process_name) {
  int max_tries = 10;
  while (!ProcessExists(process_name) && (max_tries-- > 0)) {
    usleep(five_hundres_ms);
  }
}

static void WaitForProcessNotToExist(std::string process_name) {
  int max_tries = 10;
  while (ProcessExists(process_name) && (max_tries-- > 0)) {
    usleep(five_hundres_ms);
  }
}

static void KillProcessByName(std::string process_name) {
  system(("kill -9 $(pidof " + process_name + ")").c_str());
  WaitForProcessNotToExist(process_name);
}

static pid_t ExecuteInNewProcess(const std::function<void()>& func) {
  pid_t pid = fork();
  if (pid == 0) {
    func();
    exit(0);
  }

  return pid;
}

class DaemonizerTest : public ScpTestBase {};

TEST_F(DaemonizerTest, ShouldStartProcess) {
  std::string process_name = GetRandomProcessName();
  std::string cmd = GetCommandToStartNamedProcess(process_name);
  char* args[] = {const_cast<char*>(cmd.c_str())};

  DaemonizerForTests d(1, args);
  // This ensures the daemonizer process does not restart the process
  d.StopAfterThisManyRestarts(0);
  // d.Run() should start a process with name process_name
  pid_t daemonizer_proc_id = ExecuteInNewProcess([&] { d.Run(); });

  WaitForProcessToExist(process_name);

  // This process should exist, as the daemonizer should have started it
  if (!ProcessExists(process_name)) {
    FAIL() << "The daemonizer failed to start the child process";
  }

  // As cleanup, kill the process we started
  KillProcessByName(process_name);
  // Make sure the daemonizer process exited
  waitpid(daemonizer_proc_id, NULL, 0);
}

TEST_F(DaemonizerTest, ShouldStartMultipleProcesses) {
  std::string process_name1 = GetRandomProcessName();
  std::string cmd1 = GetCommandToStartNamedProcess(process_name1);
  std::string process_name2 = GetRandomProcessName();
  std::string cmd2 = GetCommandToStartNamedProcess(process_name2);
  char* args[] = {const_cast<char*>(cmd1.c_str()),
                  const_cast<char*>(cmd2.c_str())};

  DaemonizerForTests d(2, args);
  // This ensures the daemonizer process does not restart the processes
  d.StopAfterThisManyRestarts(0);
  // d.Run() should start processes with name process_name1 and process_name2
  pid_t daemonizer_proc_id = ExecuteInNewProcess([&] { d.Run(); });

  // Both processes should exist, as the daemonizer should have started them
  WaitForProcessToExist(process_name1);
  WaitForProcessToExist(process_name2);
  if (!ProcessExists(process_name1)) {
    FAIL() << "The daemonizer failed to start the child process 1";
  }
  if (!ProcessExists(process_name2)) {
    FAIL() << "The daemonizer failed to start the child process 2";
  }

  // As cleanup, kill the processes we started
  KillProcessByName(process_name1);
  KillProcessByName(process_name2);
  // Make sure the daemonizer process exited
  waitpid(daemonizer_proc_id, NULL, 0);
}

TEST_F(DaemonizerTest, ShouldRestartProcessIfItDies) {
  std::string process_name = GetRandomProcessName();
  std::string cmd = GetCommandToStartNamedProcess(process_name);
  char* args[] = {const_cast<char*>(cmd.c_str())};

  DaemonizerForTests d(1, args);
  // This ensures the daemonizer process restarts the process only once
  d.StopAfterThisManyRestarts(1);
  // d.Run() should start a process with name process_name
  pid_t daemonizer_proc_id = ExecuteInNewProcess([&] { d.Run(); });

  WaitForProcessToExist(process_name);

  if (!ProcessExists(process_name)) {
    FAIL() << "The daemonizer failed to start the child process";
  }

  // Kill the process
  KillProcessByName(process_name);

  // Daemonizer should have restarted the process
  WaitForProcessToExist(process_name);
  if (!ProcessExists(process_name)) {
    FAIL() << "The daemonizer failed to restart the child process";
  }

  // As cleanup, kill the process we started
  KillProcessByName(process_name);
  // Make sure the daemonizer process exited
  waitpid(daemonizer_proc_id, NULL, 0);
}

TEST_F(DaemonizerTest, ShouldRestartMultipleProcesses) {
  std::string process_name1 = GetRandomProcessName();
  std::string cmd1 = GetCommandToStartNamedProcess(process_name1);
  std::string process_name2 = GetRandomProcessName();
  std::string cmd2 = GetCommandToStartNamedProcess(process_name2);
  char* args[] = {const_cast<char*>(cmd1.c_str()),
                  const_cast<char*>(cmd2.c_str())};

  DaemonizerForTests d(2, args);
  // This ensures the daemonizer process restarts each process only once
  d.StopAfterThisManyRestarts(2);
  // d.Run() should start processes with name process_name1 and process_name2
  pid_t daemonizer_proc_id = ExecuteInNewProcess([&] { d.Run(); });

  // Both processes should exist, as the daemonizer should have started them
  WaitForProcessToExist(process_name1);
  WaitForProcessToExist(process_name2);
  if (!ProcessExists(process_name1)) {
    FAIL() << "The daemonizer failed to start the child process 1";
  }
  if (!ProcessExists(process_name2)) {
    FAIL() << "The daemonizer failed to start the child process 2";
  }

  // Kill the processes since we want daemonizer to restart them
  KillProcessByName(process_name1);
  KillProcessByName(process_name2);

  // Both processes should exist, as the daemonizer should have restarted them
  WaitForProcessToExist(process_name1);
  WaitForProcessToExist(process_name2);
  if (!ProcessExists(process_name1)) {
    FAIL() << "The daemonizer failed to restart the child process 1";
  }
  if (!ProcessExists(process_name2)) {
    FAIL() << "The daemonizer failed to restart the child process 2";
  }

  // As cleanup, kill the processes we started
  KillProcessByName(process_name1);
  KillProcessByName(process_name2);
  // Make sure the daemonizer process exited
  waitpid(daemonizer_proc_id, NULL, 0);
}

TEST_F(DaemonizerTest, RunShouldFailIfInvalidArgs) {
  char* args[0];
  DaemonizerForTests d(0, args);

  auto result = d.Run();

  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(DAEMONIZER_INVALID_INPUT)));
}

TEST_F(DaemonizerTest, ShouldBuildExecutableArgsVector) {
  json arg1;
  arg1["executable_name"] = "exe_1";
  arg1["command_line_args"] = std::vector<std::string>{"arg1", "arg2", "arg3"};
  json arg2;
  arg2["executable_name"] = "exe_2";
  arg2["command_line_args"] =
      std::vector<std::string>{"2arg1", "2arg2", "2arg3"};

  auto arg1_str = arg1.dump();
  auto arg2_str = arg2.dump();

  char* args[] = {const_cast<char*>(arg1_str.c_str()),
                  const_cast<char*>(arg2_str.c_str())};

  DaemonizerForTests d(2, args);

  auto parsed_args = d.GetExecutableArguments();

  ASSERT_EQ("exe_1", parsed_args.at(0)->executable_name);
  ASSERT_THAT(parsed_args.at(0)->command_line_args,
              testing::ElementsAre("arg1", "arg2", "arg3"));

  ASSERT_EQ("exe_2", parsed_args.at(1)->executable_name);
  ASSERT_THAT(parsed_args.at(1)->command_line_args,
              testing::ElementsAre("2arg1", "2arg2", "2arg3"));
}

TEST_F(DaemonizerTest, ShouldNotRestartProcessIfConfigured) {
  std::string process_name1 = GetRandomProcessName();
  std::string cmd1 = GetCommandToStartNamedProcess(process_name1);
  std::string process_name2 = GetRandomProcessName();
  std::string cmd2 =
      GetCommandToStartNamedProcess(process_name2, false /*restart*/);
  char* args[] = {const_cast<char*>(cmd1.c_str()),
                  const_cast<char*>(cmd2.c_str())};

  DaemonizerForTests d(2, args);
  // This ensures the daemonizer process restarts each process only once
  d.StopAfterThisManyRestarts(2);
  // d.Run() should start processes with name process_name1 and process_name2
  pid_t daemonizer_proc_id = ExecuteInNewProcess([&] { d.Run(); });

  // Both processes should exist, as the daemonizer should have started them
  WaitForProcessToExist(process_name1);
  WaitForProcessToExist(process_name2);
  if (!ProcessExists(process_name1)) {
    FAIL() << "The daemonizer failed to start the child process 1";
  }
  if (!ProcessExists(process_name2)) {
    FAIL() << "The daemonizer failed to start the child process 2";
  }

  // Kill the processes since we want daemonizer to restart them
  KillProcessByName(process_name1);
  KillProcessByName(process_name2);

  // Process 1 should exist, as the daemonizer should have restarted it
  WaitForProcessToExist(process_name1);
  if (!ProcessExists(process_name1)) {
    FAIL() << "The daemonizer failed to restart the child process 1";
  }

  // Process 2 should not exist even after some wait, as the daemonizer should
  // not restart it
  WaitForProcessToExist(process_name2);
  if (ProcessExists(process_name2)) {
    FAIL() << "The daemonizer should not restart the child process 2";
  }

  // As cleanup, kill the process we started
  KillProcessByName(process_name1);
  // Make sure the daemonizer process exited
  waitpid(daemonizer_proc_id, NULL, 0);
}
}  // namespace google::scp::process_launcher::test
