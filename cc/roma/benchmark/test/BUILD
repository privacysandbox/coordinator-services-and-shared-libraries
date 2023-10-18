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

cc_test(
    name = "benchmark_suite_test",
    timeout = "eternal",
    srcs = ["benchmark_suite_test.cc"],
    copts = [
        "-std=c++17",
    ],
    tags = ["manual"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/roma/benchmark/src:roma_benchmark_lib",
        "//cc/roma/roma_service/src:roma_service_lib",
        "//cc/roma/sandbox/constants:roma_constants_lib",
        "@google_benchmark//:benchmark",
    ],
)

cc_test(
    name = "benchmark_test",
    size = "large",
    srcs = ["benchmark_test.cc"],
    copts = [
        "-std=c++17",
    ],
    tags = ["manual"],
    deps = [
        "//cc/core/common/time_provider/src:time_provider_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/roma/roma_service/src:roma_service_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
