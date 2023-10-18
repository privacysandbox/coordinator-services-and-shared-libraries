# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "test_pbs_server_starter_lib",
    testonly = True,
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
    ),
    copts = [
        "-std=c++17",
    ],
    data = [
        "//cc/pbs/deploy/pbs_server/build_defs:pbs_container_aws.tar",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/core/test/utils/aws_helper:aws_helper_lib",
        "//cc/core/test/utils/docker_helper:docker_helper_lib",
        "//cc/pbs/interface:pbs_interface_lib",
        "//cc/pbs/pbs_server/src/cloud_platform_dependency_factory/aws_integration_test:pbs_aws_integration_test_cloud_platform_dependency_factory_lib",
        "@aws_sdk_cpp//:dynamodb",
        "@aws_sdk_cpp//:s3",
    ],
)
