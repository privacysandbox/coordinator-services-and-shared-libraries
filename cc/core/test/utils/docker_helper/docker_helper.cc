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

#include <iostream>
#include <map>
#include <string>

#include "absl/strings/str_format.h"

using std::map;
using std::string;

static constexpr char kLocalstackImage[] = "localstack/localstack:1.0.3";

namespace google::scp::core::test {
string PortMapToSelf(string port) {
  return port + ":" + port;
}

int StartLocalStackContainer(const string& network,
                             const string& container_name,
                             const string& exposed_port) {
  map<string, string> env_variables;
  env_variables["EDGE_PORT"] = exposed_port;
  // localstack version is pinned so that tests are repeatable
  return StartContainer(network, container_name, kLocalstackImage,
                        PortMapToSelf(exposed_port), "4510-4559",
                        env_variables);
}

int StartContainer(
    const string& network, const string& container_name,
    const string& image_name, const string& port_mapping1,
    const string& port_mapping2,
    const std::map<std::string, std::string>& environment_variables) {
  return std::system(BuildStartContainerCmd(network, container_name, image_name,
                                            port_mapping1, port_mapping2,
                                            environment_variables)
                         .c_str());
}

string BuildStartContainerCmd(
    const string& network, const string& container_name,
    const string& image_name, const string& port_mapping1,
    const string& port_mapping2,
    const std::map<std::string, std::string>& environment_variables) {
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
      "%s",
      name_network, container_name, ports_mapping, envs, image_name);
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
  return absl::StrFormat("docker stop %s", container_name);
}
}  // namespace google::scp::core::test
