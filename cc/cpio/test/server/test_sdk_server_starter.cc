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

#include "test_sdk_server_starter.h"

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "core/test/utils/docker_helper/docker_helper.h"

using google::scp::core::test::CreateNetwork;
using google::scp::core::test::LoadImage;
using google::scp::core::test::PortMapToSelf;
using google::scp::core::test::RemoveNetwork;
using google::scp::core::test::StartContainer;
using google::scp::core::test::StopContainer;
using std::map;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::vector;

namespace google::scp::cpio::test {
void TestSdkServerStarter::RunSdkServer(
    const string& image_location, const string& image_name,
    const std::map<std::string, std::string>& env_overrides) {
  std::cout << "Loading SDK image" << std::endl;
  if (LoadImage(image_location) != 0) {
    throw runtime_error("Failed to load SDK image!");
  }

  std::cout << "Starting SDK server" << std::endl;
  auto env = CreateSdkEnvVariables();
  for (auto& env_override : env_overrides) {
    env[env_override.first] = env_override.second;
  }
  // Mount /tmp folder to make the unix socket address available outside the
  // container.
  if (StartContainer(config_.network_name, config_.sdk_container_name,
                     image_name, PortMapToSelf(config_.sdk_port), "", env,
                     "-v /tmp:/tmp:rw")) {
    throw runtime_error("Failed to start SDK container!");
  }
}

void TestSdkServerStarter::Setup() {
  // Creates network.
  if (CreateNetwork(config_.network_name) != 0) {
    throw runtime_error("Failed to create network!");
  }

  RunCloud();

  // Wait for the server to be up.
  std::this_thread::sleep_for(std::chrono::seconds(5));
}

void TestSdkServerStarter::StopSdkServer() {
  if (StopContainer(config_.sdk_container_name) != 0) {
    throw runtime_error("Failed to stop SDK container!");
  }
}

void TestSdkServerStarter::Teardown() {
  if (StopContainer(config_.cloud_container_name) != 0) {
    throw runtime_error("Failed to stop cloud container!");
  }
  std::this_thread::sleep_for(std::chrono::seconds(5));
  if (RemoveNetwork(config_.network_name) != 0) {
    throw runtime_error("Failed to remove network!");
  }
}
}  // namespace google::scp::cpio::test
