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

#pragma once

#include <map>
#include <string>

#include "absl/strings/str_format.h"

namespace google::scp::core::test {
std::string PortMapToSelf(std::string port);

int StartLocalStackContainer(const std::string& network,
                             const std::string& container_name,
                             const std::string& exposed_port);

int StartGcpContainer(const std::string& network,
                      const std::string& container_name,
                      const std::string& exposed_port);

int StartContainer(
    const std::string& network, const std::string& container_name,
    const std::string& image_name, const std::string& port_mapping1,
    const std::string& port_mapping2 = "",
    const std::map<std::string, std::string>& environment_variables =
        std::map<std::string, std::string>({}),
    const std::string& addition_args = "");

int CreateImage(const std::string& image_target, const std::string& args = "");

int LoadImage(const std::string& image_name);

int CreateNetwork(const std::string& network_name);

int RemoveNetwork(const std::string& network_name);

int StopContainer(const std::string& container_name);

std::string BuildStopContainerCmd(const std::string& container_name);

std::string BuildRemoveNetworkCmd(const std::string& network_name);

std::string BuildCreateNetworkCmd(const std::string& network_name);

std::string BuildLoadImageCmd(const std::string& image_name);

std::string BuildCreateImageCmd(const std::string& image_target,
                                const std::string& args = "");

std::string BuildStartContainerCmd(
    const std::string& network, const std::string& container_name,
    const std::string& image_name, const std::string& port_mapping1,
    const std::string& port_mapping2 = "",
    const std::map<std::string, std::string>& environment_variables =
        std::map<std::string, std::string>({}),
    const std::string& addition_args = "");

/**
 * @brief Get the Ip Address of a docker container.
 *
 * @param network_name the network the container is in.
 * @param container_name the container name.
 * @return std::string the returned IP address.
 */
std::string GetIpAddress(const std::string& network_name,
                         const std::string& container_name);

/**
 * @brief Run docker command to grant 666 permission to the given folder inside
 * the given container.
 *
 * @param container_name the name of the given container.
 * @param folder the given folder.
 */
void GrantPermissionToFolder(const std::string& container_name,
                             const std::string& folder);
}  // namespace google::scp::core::test
