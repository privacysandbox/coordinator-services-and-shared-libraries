# Copyright 2023 Google LLC
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
    name = "worker_pool_api_sapi_test",
    size = "small",
    srcs = ["worker_pool_api_sapi_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/roma/sandbox/worker_api/src:roma_worker_api_sapi_lib",
        "//cc/roma/sandbox/worker_pool/src:roma_worker_pool_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
