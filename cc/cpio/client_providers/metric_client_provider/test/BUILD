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

load("@rules_cc//cc:defs.bzl", "cc_test")

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_test(
    name = "metric_client_provider_test",
    size = "small",
    srcs = ["metric_client_provider_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/message_router/src:message_router_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/metric_client_provider/mock:metric_client_provider_mock",
        "//cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
        "//cc/cpio/common/src/aws:aws_utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "metric_client_utils_test",
    size = "small",
    srcs = ["metric_client_utils_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)
