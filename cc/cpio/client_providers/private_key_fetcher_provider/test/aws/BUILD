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

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_test(
    name = "aws_private_key_fetcher_provider_test",
    size = "small",
    srcs = ["aws_private_key_fetcher_provider_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/http2_client/mock:http2_client_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/cpio/client_providers/private_key_fetcher_provider/src/aws:aws_private_key_fetcher_provider_lib",
        "//cc/cpio/client_providers/role_credentials_provider/mock:role_credentials_provider_mock",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@aws_sdk_cpp//:core",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "test_aws_private_key_fetcher_provider_lib",
    srcs = [
        "test_aws_private_key_fetcher_provider.cc",
        "test_aws_private_key_fetcher_provider.h",
        # Need to include the production src here,
        # otherwise, it will fail with duplicate definition for test server.
        "//cc/cpio/client_providers/private_key_fetcher_provider/src/aws:aws_private_key_fetcher_provider_srcs",
    ],
    copts = [
        "-DTEST_CPIO=1",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/cpio/client_providers/private_key_fetcher_provider/src/aws:aws_private_key_fetcher_provider_lib",
    ],
)
