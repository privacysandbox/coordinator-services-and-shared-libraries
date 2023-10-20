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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")
load("//cc/public/cpio/build_deps/shared:sdk_container.bzl", "sdk_container")

package(default_visibility = ["//cc:scp_internal_pkg"])

sdk_container(
    name = "test_aws_sdk_server",
    inside_tee = False,
    is_test_server = True,
    platform = "aws",
    recover_client_binaries = False,
    recover_sdk_binaries = False,
    sdk_binaries =
        {
            "blob_storage_service": {
            },
            "metric_service": {
            },
            "parameter_service": {
            },
        },
)

cc_library(
    name = "test_aws_sdk_server_starter_lib",
    testonly = True,
    srcs = glob(
        [
            "test_aws_sdk_server_starter.cc",
            "test_aws_sdk_server_starter.h",
        ],
    ),
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/core/test/utils/aws_helper:aws_helper_lib",
        "//cc/core/test/utils/docker_helper:docker_helper_lib",
        "//cc/cpio/server/interface:server_interface_lib",
        "//cc/cpio/server/src/blob_storage_service/test_aws:test_aws_blob_storage_service_factory_lib",
        "//cc/cpio/server/src/metric_service/test_aws:test_aws_metric_service_factory_lib",
        "//cc/cpio/server/src/parameter_service/test_aws:test_aws_parameter_service_factory_lib",
        "//cc/cpio/test/server:test_sdk_server_starter_lib",
    ],
)

# To run this test:
# cc_test "--//cc/public/cpio/interface:platform=aws \
#  --//cc/cpio/server/interface:is_test_server=True \
#  --//cc/public/cpio/interface:run_inside_tee=False \
#  cc/cpio/test/server/aws:aws_sdk_server_integration_test"
cc_test(
    name = "aws_sdk_server_integration_test",
    size = "large",
    timeout = "long",
    srcs = ["aws_sdk_server_integration_test.cc"],
    data = [
        "//cc/cpio/test/server/aws:test_aws_sdk_server_container.tar",
    ],
    tags = ["manual"],
    deps = [
        ":test_aws_sdk_server_starter_lib",
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/interface:logger_interface",
        "//cc/core/logger/src:logger_lib",
        "//cc/core/logger/src/log_providers:log_providers_lib",
        "//cc/core/logger/src/log_providers/syslog:syslog_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/server/interface:server_interface_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/proto/blob_storage_service/v1:blob_storage_service_grpc",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_grpc",
        "//cc/public/cpio/proto/parameter_service/v1:parameter_service_cc_grpc",
        "@aws_sdk_cpp//:core",
        "@com_github_grpc_grpc//:grpc++_reflection",  # for grpc_cli
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
