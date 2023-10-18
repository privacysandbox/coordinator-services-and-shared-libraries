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

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "serializer_test",
    size = "small",
    srcs = ["serializer_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/roma/wasm/src:roma_wasm_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "deserializer_test",
    size = "small",
    srcs = ["deserializer_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/roma/wasm/src:roma_wasm_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "serde_test",
    size = "small",
    srcs = ["serde_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc/core/test/utils:utils_lib",
        "//cc/roma/wasm/src:roma_wasm_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "roma_wasm_testing_lib",
    srcs =
        [
            "testing_utils.cc",
            "testing_utils.h",
        ],
    copts = [
        "-std=c++17",
    ],
    deps = ["@com_google_absl//absl/log:check"],
)
