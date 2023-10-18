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
    name = "worker_sandbox_api_test",
    size = "small",
    srcs = ["worker_sandbox_api_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/roma/sandbox/constants:roma_constants_lib",
        "//cc/roma/sandbox/worker_api/sapi/src:roma_worker_sandbox_api_lib",
        "//cc/roma/sandbox/worker_factory/src:roma_worker_factory_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "worker_wrapper_test",
    size = "small",
    srcs = ["worker_wrapper_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/roma/sandbox/constants:roma_constants_lib",
        "//cc/roma/sandbox/worker_api/sapi/src:roma_worker_wrapper_lib",
        "//cc/roma/sandbox/worker_api/sapi/src:worker_init_params_cc_proto",
        "//cc/roma/sandbox/worker_factory/src:roma_worker_factory_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
