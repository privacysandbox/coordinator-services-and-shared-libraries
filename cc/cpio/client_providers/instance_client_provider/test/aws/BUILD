# Copyright 2022 Google LLC
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

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_test(
    name = "ec2_error_converter_test",
    size = "small",
    srcs =
        ["ec2_error_converter_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/client_providers/instance_client_provider/src:instance_client_provider_lib",
        "//cc/cpio/common/src/aws:aws_utils_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "aws_instance_client_provider_test",
    size = "small",
    timeout = "long",
    srcs =
        ["aws_instance_client_provider_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/client_providers/instance_client_provider/mock/aws:aws_instance_client_provider_mock",
        "//cc/cpio/client_providers/instance_client_provider/src/aws:aws_instance_client_provider_lib",
        "//cc/cpio/common/src/aws:aws_utils_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
