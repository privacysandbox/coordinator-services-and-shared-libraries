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

#include "test_aws_sdk_server_starter.h"

#include <chrono>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "absl/strings/str_format.h"
#include "core/test/utils/aws_helper/aws_helper.h"
#include "core/test/utils/docker_helper/docker_helper.h"
#include "cpio/server/interface/configuration_keys.h"
#include "cpio/server/src/blob_storage_service/test_aws/test_configuration_keys.h"
#include "cpio/server/src/metric_service/test_aws/test_configuration_keys.h"
#include "cpio/server/src/parameter_service/test_aws/test_configuration_keys.h"

using Aws::Map;
using Aws::String;
using google::scp::core::test::StartLocalStackContainer;
using std::map;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::vector;

namespace google::scp::cpio::test {
void TestAwsSdkServerStarter::RunCloud() {
  // Starts localstack.
  if (StartLocalStackContainer(config_.network_name,
                               config_.cloud_container_name,
                               config_.cloud_port) != 0) {
    throw runtime_error("Failed to start localstack container!");
  }
}

map<string, string> TestAwsSdkServerStarter::CreateSdkEnvVariables() {
  map<string, string> env_variables;
  string cloud_endpoint_in_container =
      "http://" + config_.cloud_container_name + ":" + config_.cloud_port;

  env_variables[kSdkClientLogOption] = "ConsoleLog";
  env_variables[kTestMetricClientCloudEndpiontOverride] =
      cloud_endpoint_in_container;
  env_variables[kTestBlobStorageClientCloudEndpiontOverride] =
      cloud_endpoint_in_container;
  env_variables[kTestParameterClientCloudEndpiontOverride] =
      cloud_endpoint_in_container;

  return env_variables;
}
}  // namespace google::scp::cpio::test
