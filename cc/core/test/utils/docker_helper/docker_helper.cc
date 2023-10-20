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

#include "docker_helper.h"

#include <chrono>
#include <cstdio>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

using std::array;
using std::map;
using std::runtime_error;
using std::string;

// localstack version is pinned so that tests are repeatable
static constexpr char kLocalstackImage[] = "localstack/localstack:1.0.3";
// gcloud SDK tool version is pinned so that tests are repeatable
static constexpr char kGcpImage[] =
    "gcr.io/google.com/cloudsdktool/google-cloud-cli:380.0.0-emulators";

namespace google::scp::core::test {
string PortMapToSelf(string port) {
  return port + ":" + port;
}

int StartLocalStackContainer(const string& network,
                             const string& container_name,
                             const string& exposed_port) {
  map<string, string> env_variables;
  env_variables["EDGE_PORT"] = exposed_port;
  return StartContainer(network, container_name, kLocalstackImage,
                        PortMapToSelf(exposed_port), "4510-4559",
                        env_variables);
}

int StartGcpContainer(const string& network, const string& container_name,
                      const string& exposed_port) {
  map<string, string> env_variables;
  return StartContainer(network, container_name, kGcpImage,
                        PortMapToSelf(exposed_port), "9000-9050",
                        env_variables);
}

int StartContainer(
    const string& network, const string& container_name,
    const string& image_name, const string& port_mapping1,
    const string& port_mapping2,
    const std::map<std::string, std::string>& environment_variables,
    const string& addition_args) {
  return std::system(BuildStartContainerCmd(
                         network, container_name, image_name, port_mapping1,
                         port_mapping2, environment_variables, addition_args)
                         .c_str());
}

string BuildStartContainerCmd(
    const string& network, const string& container_name,
    const string& image_name, const string& port_mapping1,
    const string& port_mapping2,
    const std::map<std::string, std::string>& environment_variables,
    const string& addition_args) {
  auto ports_mapping = absl::StrFormat("-p %s ", port_mapping1);
  if (!port_mapping2.empty()) {
    ports_mapping += absl::StrFormat("-p %s ", port_mapping2);
  }

  string name_network;
  if (!network.empty()) {
    name_network = absl::StrFormat("--network=%s ", network);
  }

  string envs;
  for (auto it = environment_variables.begin();
       it != environment_variables.end(); ++it) {
    envs += absl::StrFormat("--env %s=%s ", it->first, it->second);
  }

  return absl::StrFormat(
      "docker -D run --rm -itd --privileged "
      "%s"
      "--name=%s "
      "%s"
      "%s"
      "%s"
      "%s",
      name_network, container_name, ports_mapping, envs,
      addition_args.empty() ? addition_args : addition_args + " ", image_name);
}

int CreateImage(const string& image_target, const string& args) {
  return std::system(BuildCreateImageCmd(image_target, args).c_str());
}

string BuildCreateImageCmd(const string& image_target, const string& args) {
  auto cmd = absl::StrFormat(
      "bazel build --action_env=BAZEL_CXXOPTS='-std=c++17' %s", image_target);
  if (!args.empty()) {
    cmd += " " + args;
  }
  return cmd;
}

int LoadImage(const std::string& image_name) {
  return std::system(BuildLoadImageCmd(image_name).c_str());
}

string BuildLoadImageCmd(const std::string& image_name) {
  return absl::StrFormat("docker load < %s", image_name);
}

int CreateNetwork(const std::string& network_name) {
  return std::system(BuildCreateNetworkCmd(network_name).c_str());
}

std::string BuildCreateNetworkCmd(const std::string& network_name) {
  return absl::StrFormat("docker network create %s", network_name);
}

int RemoveNetwork(const std::string& network_name) {
  return std::system(BuildRemoveNetworkCmd(network_name).c_str());
}

std::string BuildRemoveNetworkCmd(const std::string& network_name) {
  return absl::StrFormat("docker network rm %s", network_name);
}

int StopContainer(const std::string& container_name) {
  return std::system(BuildStopContainerCmd(container_name).c_str());
}

std::string BuildStopContainerCmd(const std::string& container_name) {
  return absl::StrFormat("docker rm -f %s", container_name);
}

std::string GetIpAddress(const std::string& network_name,
                         const std::string& container_name) {
  char buffer[20];
  string result;
  string command =
      absl::StrCat("docker inspect -f '{{ .NetworkSettings.Networks.",
                   network_name, ".IPAddress }}' ", container_name);
  auto pipe = popen(command.c_str(), "r");
  if (!pipe) {
    throw runtime_error("Failed to fetch IP address for container!");
    return "";
  }

  while (fgets(buffer, 20, pipe) != nullptr) {
    result += buffer;
  }

  auto return_status = pclose(pipe);
  if (return_status == EXIT_FAILURE) {
    throw runtime_error(
        "Failed to close pipe to fetch IP address for container!");
    return "";
  }

  auto length = result.length();
  if (result.back() == '\n') {
    length -= 1;
  }
  return result.substr(0, length);
}

void GrantPermissionToFolder(const string& container_name,
                             const string& folder) {
  string s = absl::StrCat("docker exec -itd " + container_name + " chmod 666 " +
                          folder);
  auto result = std::system(s.c_str());
  if (result != 0) {
    throw runtime_error("Failed to grant permission!");
  } else {
    std::cout << "Succeeded to grant permission!" << std::endl;
  }
}
}  // namespace google::scp::core::test
