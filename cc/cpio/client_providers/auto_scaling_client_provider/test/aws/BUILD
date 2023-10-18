# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_test(
    name = "auto_scaling_error_converter_test",
    size = "small",
    srcs =
        ["auto_scaling_error_converter_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/client_providers/auto_scaling_client_provider/src/aws:aws_auto_scaling_client_provider_lib",
        "//cc/cpio/common/src/aws:aws_utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@aws_sdk_cpp//:autoscaling",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "aws_auto_scaling_client_provider_test",
    size = "small",
    timeout = "long",
    srcs = [
        "aws_auto_scaling_client_provider_test.cc",
    ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/auto_scaling_client_provider/mock/aws:aws_auto_scaling_client_provider_mock",
        "//cc/cpio/client_providers/auto_scaling_client_provider/src/aws:aws_auto_scaling_client_provider_lib",
        "//cc/cpio/client_providers/instance_client_provider/mock:instance_client_provider_mock",
        "//cc/cpio/client_providers/instance_client_provider/src/aws:aws_instance_client_provider_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/proto/auto_scaling_service/v1:auto_scaling_service_cc_proto",
        "@aws_sdk_cpp//:autoscaling",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "test_aws_auto_scaling_client_provider_lib",
    srcs = [
        "test_aws_auto_scaling_client_provider.cc",
        "test_aws_auto_scaling_client_provider.h",
        "//cc/cpio/client_providers/auto_scaling_client_provider/src/aws:aws_auto_scaling_client_provider_srcs",
    ],
    copts = [
        "-DTEST_CPIO=1",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/cpio/client_providers/auto_scaling_client_provider/src/aws:aws_auto_scaling_client_provider_lib",
        "//cc/cpio/common/test/aws:test_aws_utils_lib",
    ],
)
