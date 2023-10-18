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
    name = "test_gcp_sdk_server",
    inside_tee = False,
    is_test_server = True,
    platform = "gcp",
    recover_client_binaries = False,
    recover_sdk_binaries = True,
    sdk_binaries =
        {
            "queue_service": {
            },
        },
)

cc_library(
    name = "test_gcp_sdk_server_starter_lib",
    testonly = True,
    srcs = glob(
        [
            "test_gcp_sdk_server_starter.cc",
            "test_gcp_sdk_server_starter.h",
        ],
    ),
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/core/test/utils/docker_helper:docker_helper_lib",
        "//cc/cpio/server/interface:server_interface_lib",
        "//cc/cpio/server/interface/queue_service:queue_service_interface_lib",
        "//cc/cpio/server/src/queue_service/test_gcp:test_gcp_queue_service_factory_lib",
        "//cc/cpio/test/server:test_sdk_server_starter_lib",
    ],
)

cc_test(
    name = "gcp_sdk_server_integration_test",
    size = "small",
    timeout = "long",
    srcs = ["gcp_sdk_server_integration_test.cc"],
    data = [
        "//cc/cpio/test/server/gcp:test_gcp_sdk_server_container.tar",
    ],
    tags = ["manual"],
    deps = [
        ":test_gcp_sdk_server_starter_lib",
        "//cc:cc_base_include_dir",
        "//cc/core/test/utils:utils_lib",
        "//cc/core/test/utils/docker_helper:docker_helper_lib",
        "//cc/core/test/utils/gcp_helper:gcp_helper_lib",
        "//cc/public/core/interface:execution_result",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/interface:cpio_errors",
        "//cc/public/cpio/proto/queue_service/v1:queue_service_cc_grpc",
        "//cc/public/cpio/proto/queue_service/v1:queue_service_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)
