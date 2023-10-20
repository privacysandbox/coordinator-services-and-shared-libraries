// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "core/tcp_traffic_forwarder/src/tcp_traffic_forwarder_socat.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <random>
#include <string>
#include <thread>
#include <vector>

#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::TCPTrafficForwarderSocat;
using std::cout;
using std::make_shared;
using std::move;
using std::shared_ptr;
using std::string;
using std::thread;
using std::to_string;
using std::vector;
using ::testing::HasSubstr;

namespace google::scp::core::tcp_traffic_forwarder::test {
static void SigChildHandler(int sig) {
  pid_t pid;
  int status;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {}
}

class TcpTrafficForwarderSoCatTest : public ::testing::Test {
 public:
  void SetUp() {
    struct sigaction sigchild_action {};

    sigchild_action.sa_handler = SigChildHandler;
    sigaction(SIGCHLD, &sigchild_action, nullptr);
  }
};

static int GenerateRandomIntInRange(int min, int max) {
  std::random_device random_device;
  std::mt19937 random_number_engine(random_device());
  std::uniform_int_distribution<int> uniform_distribution(min, max);

  return uniform_distribution(random_number_engine);
}

static std::string RunCommandAndGetOutput(const string& cmd) {
  char line_buffer[256];
  std::string result = "";
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  try {
    while (fgets(line_buffer, sizeof(line_buffer), pipe) != NULL) {
      result += line_buffer;
    }
  } catch (...) {
    pclose(pipe);
    throw;
  }
  pclose(pipe);
  return result;
}

static bool SocatProcessExists() {
  // Find all the processes that have "socat" in the command. Exclude this grep
  // search from the return.
  auto result =
      RunCommandAndGetOutput("ps xao comm | grep 'socat' | grep -v 'grep'");
  return result.find("socat") != result.npos;
}

TEST_F(TcpTrafficForwarderSoCatTest, InitRunStop) {
  auto local_port = to_string(GenerateRandomIntInRange(8000, 60000));
  TCPTrafficForwarderSocat tcp_traffic_forwarder(local_port);
  EXPECT_SUCCESS(tcp_traffic_forwarder.Init());
  EXPECT_SUCCESS(tcp_traffic_forwarder.Run());
  EXPECT_SUCCESS(tcp_traffic_forwarder.Stop());
}

TEST_F(TcpTrafficForwarderSoCatTest, ShouldStartAndStopSocat) {
  auto local_port = to_string(GenerateRandomIntInRange(8000, 60000));
  TCPTrafficForwarderSocat tcp_traffic_forwarder(local_port);
  EXPECT_SUCCESS(tcp_traffic_forwarder.Init());
  // This calls Run()
  EXPECT_SUCCESS(
      tcp_traffic_forwarder.ResetForwardingAddress("www.google.com"));
  // Socat should have been started
  while (!SocatProcessExists()) {}
  EXPECT_SUCCESS(tcp_traffic_forwarder.Stop());
  // No socat processes should be running
  while (SocatProcessExists()) {}
}

TEST_F(TcpTrafficForwarderSoCatTest, ShouldForwardBasicTraffic) {
  auto local_port = to_string(GenerateRandomIntInRange(8000, 60000));
  auto command = "curl localhost:" + local_port + "> /dev/null";

  TCPTrafficForwarderSocat tcp_traffic_forwarder(local_port);
  EXPECT_SUCCESS(tcp_traffic_forwarder.Init());

  EXPECT_SUCCESS(
      tcp_traffic_forwarder.ResetForwardingAddress("www.google.com"));
  // Socat should have been started
  while (!SocatProcessExists()) {}

  EXPECT_SUCCESS(tcp_traffic_forwarder.Stop());

  // No socat processes should be running
  while (SocatProcessExists()) {}
}

TEST_F(TcpTrafficForwarderSoCatTest, ShouldBeAbleToResetAddress) {
  auto local_port = to_string(GenerateRandomIntInRange(8000, 60000));

  TCPTrafficForwarderSocat tcp_traffic_forwarder(local_port);
  EXPECT_SUCCESS(tcp_traffic_forwarder.Init());

  EXPECT_SUCCESS(
      tcp_traffic_forwarder.ResetForwardingAddress("www.google.com"));
  // Socat should have been started
  while (!SocatProcessExists()) {}
  EXPECT_SUCCESS(tcp_traffic_forwarder.ResetForwardingAddress("www.yahoo.com"));
  // Socat should still be running
  while (!SocatProcessExists()) {}

  EXPECT_SUCCESS(tcp_traffic_forwarder.Stop());

  // No socat processes should be running
  while (SocatProcessExists()) {}
}

TEST_F(TcpTrafficForwarderSoCatTest, ShouldForwardTraffic) {
  auto local_port = to_string(GenerateRandomIntInRange(8000, 60000));

  TCPTrafficForwarderSocat tcp_traffic_forwarder(local_port);
  EXPECT_SUCCESS(tcp_traffic_forwarder.Init());

  EXPECT_SUCCESS(
      tcp_traffic_forwarder.ResetForwardingAddress("www.google.com:80"));
  // Socat should have been started
  while (!SocatProcessExists()) {}

  auto curl_command = "curl -m 5 --retry 2 localhost:" + local_port;
  auto result = RunCommandAndGetOutput(curl_command);
  EXPECT_THAT(result, HasSubstr("<!DOCTYPE html>"));

  EXPECT_SUCCESS(tcp_traffic_forwarder.Stop());

  // No socat processes should be running
  while (SocatProcessExists()) {}
}
}  // namespace google::scp::core::tcp_traffic_forwarder::test
