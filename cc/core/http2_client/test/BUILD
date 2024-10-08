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
    name = "http2_client_test",
    size = "small",
    timeout = "moderate",
    srcs = [
        "http2_client_test.cc",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/http2_client/mock:http2_client_mock",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_github_nghttp2_nghttp2//:nghttp2",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "http2_connection_test",
    size = "small",
    timeout = "moderate",
    srcs = [
        "http2_connection_test.cc",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/http2_client/mock:http2_client_mock",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/telemetry/mock:telemetry_fake",
        "//cc/core/telemetry/mock:telemetry_mock",
        "//cc/core/telemetry/src/common:telemetry_common",
        "//cc/core/telemetry/src/common:telemetry_metric_utils",
        "//cc/core/telemetry/src/metric:telemetry_metric",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_github_nghttp2_nghttp2//:nghttp2",
        "@com_google_googletest//:gtest_main",
        "@io_opentelemetry_cpp//sdk/src/metrics",
    ],
)

cc_test(
    name = "http2_connection_pool_test",
    size = "small",
    timeout = "moderate",
    srcs = [
        "http2_connection_pool_test.cc",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/http2_client/mock:http2_client_mock",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/telemetry/mock:telemetry_fake",
        "//cc/core/telemetry/mock:telemetry_mock",
        "//cc/core/telemetry/src/common:telemetry_common",
        "//cc/core/telemetry/src/common:telemetry_metric_utils",
        "//cc/core/telemetry/src/metric:telemetry_metric",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_github_nghttp2_nghttp2//:nghttp2",
        "@com_google_googletest//:gtest_main",
        "@io_opentelemetry_cpp//sdk/src/metrics",
    ],
)

cc_test(
    name = "aws_v4_signer_test",
    size = "small",
    srcs = ["aws_v4_signer_test.cc"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_github_nghttp2_nghttp2//:nghttp2",
        "@com_google_googletest//:gtest_main",
    ],
)
