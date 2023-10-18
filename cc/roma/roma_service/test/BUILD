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
    name = "sandboxed_service_test",
    size = "large",
    srcs = ["sandboxed_service_test.cc"],
    copts = [
        "-std=c++17",
    ],
    data = [
        "//cc/roma/testing/cpp_wasm_allocate_memory",
        "//cc/roma/testing/cpp_wasm_string_in_string_out_example",
    ],
    deps = [
        "//cc/core/common/time_provider/src:time_provider_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/roma/roma_service/src:roma_service_lib",
        "//cc/roma/wasm/test:roma_wasm_testing_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
