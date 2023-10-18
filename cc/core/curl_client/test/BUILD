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
    name = "curl_wrapper_test",
    timeout = "short",
    srcs =
        [
            "http1_curl_wrapper_test.cc",
        ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/curl_client/src:http1_curl_client_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils/http1_helper:test_http1_server",
        "//cc/core/utils/src:core_utils",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "curl_client_test",
    srcs =
        [
            "http1_curl_client_test.cc",
        ],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/curl_client/src:http1_curl_client_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/core/utils/src:core_utils",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
