# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "worker_test",
    size = "small",
    srcs = ["worker_test.cc"],
    copts = [
        "-std=c++17",
    ],
    tags = ["exclusive"],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/roma/worker/src:roma_worker_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "execution_utils_test",
    size = "small",
    srcs = ["execution_utils_test.cc"],
    copts = [
        "-std=c++17",
    ],
    data = [
        "//cc/roma/testing/cpp_wasm_list_of_string_in_list_of_string_out_example",
        "//cc/roma/testing/cpp_wasm_string_in_string_out_example",
        "//cc/roma/testing/rust_wasm_list_of_string_in_list_of_string_out_example",
        "//cc/roma/testing/rust_wasm_string_in_string_out_example",
    ],
    tags = ["manual"],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/roma/wasm/src:roma_wasm_lib",
        "//cc/roma/wasm/test:roma_wasm_testing_lib",
        "//cc/roma/worker/src:roma_worker_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "execution_manager_test",
    size = "small",
    srcs = ["execution_manager_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/roma/worker/src:roma_worker_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "worker_pool_test",
    size = "medium",
    srcs = ["worker_pool_test.cc"],
    copts = [
        "-std=c++17",
    ],
    tags = ["exclusive"],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/roma/worker/src:roma_worker_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
