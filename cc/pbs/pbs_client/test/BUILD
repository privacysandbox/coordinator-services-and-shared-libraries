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

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "pbs_client_test",
    size = "small",
    srcs = ["pbs_client_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/credentials_provider/mock:core_credentials_provider_mock",
        "//cc/core/http2_client/mock:http2_client_mock",
        "//cc/core/http2_client/src:http2_client_lib",
        "//cc/core/token_provider_cache/mock:core_token_provider_cache_mock",
        "//cc/pbs/pbs_client/mock:pbs_client_mock",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
