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
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/roma/wasm/src:roma_wasm_lib",
        "//cc/roma/wasm/test:roma_wasm_testing_lib",
        "//cc/roma/worker/src:roma_execution_utils_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
