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
    name = "aws_authorizer_test",
    size = "small",
    srcs = ["aws_authorizer_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/authorization_service/mock:core_authorization_service_mock",
        "//cc/core/authorization_service/src:core_authorization_service",
        "//cc/core/common/auto_expiry_concurrent_map/mock:auto_expiry_concurrent_map_mock",
        "//cc/core/http2_client/mock:http2_client_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_github_nghttp2_nghttp2//:nghttp2",
        "@com_google_googletest//:gtest_main",
    ],
)
