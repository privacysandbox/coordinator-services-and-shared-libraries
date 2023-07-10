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
    name = "basic_e2e_test",
    size = "large",
    srcs = ["basic_e2e.cc"],
    copts = [
        "-std=c++17",
    ],
    data = [
        "//cc/roma/testing/cpp_wasm_allocate_memory",
        "//cc/roma/testing/cpp_wasm_list_of_string_in_list_of_string_out_example",
        "//cc/roma/testing/cpp_wasm_return_bad_pointer",
        "//cc/roma/testing/cpp_wasm_string_in_string_out_example",
        "//cc/roma/testing/rust_wasm_list_of_string_in_list_of_string_out_example",
        "//cc/roma/testing/rust_wasm_string_in_string_out_example",
    ],
    tags = ["exclusive"],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/roma/roma_service/src:roma_service_lib",
        "//cc/roma/wasm/test:roma_wasm_testing_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "basic_e2e_wasm_memory_constraint_test",
    size = "large",
    srcs = ["basic_e2e_wasm_memory_constraint_test.cc"],
    copts = [
        "-std=c++17",
    ],
    data = [
        "//cc/roma/testing/cpp_wasm_allocate_memory",
    ],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/roma/roma_service/src:roma_service_lib",
        "//cc/roma/wasm/test:roma_wasm_testing_lib",
        "//cc/roma/worker/src:roma_worker_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "basic_e2e_worker_recovery_test",
    size = "large",
    srcs = ["basic_e2e_worker_recovery_test.cc"],
    copts = [
        "-std=c++17",
    ],
    tags = ["exclusive"],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/roma/roma_service/src:roma_service_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
