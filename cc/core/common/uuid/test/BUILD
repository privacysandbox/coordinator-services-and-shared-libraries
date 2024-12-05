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
load("//build_defs/cc:benchmark.bzl", "BENCHMARK_COPT")

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_test(
    name = "uuid_test",
    size = "small",
    srcs = [
        "uuid_test.cc",
    ],
    deps = [
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

# Run on (12 X 4346.94 MHz CPU s)
# CPU Caches:
#   L1 Data 32 KiB (x6)
#   L1 Instruction 32 KiB (x6)
#   L2 Unified 1024 KiB (x6)
#   L3 Unified 8448 KiB (x1)
# Load Average: 0.70, 0.77, 0.92
# -------------------------------------------------------------------
# Benchmark                         Time             CPU   Iterations
# -------------------------------------------------------------------
# BM_UuidFromString/1             140 ns          140 ns      4383673
# BM_UuidFromString/8            1108 ns         1108 ns       627080
# BM_UuidFromString/64           8879 ns         8878 ns        79709
# BM_UuidFromString/512         70148 ns        70143 ns         9981
# BM_UuidFromString/4096       566055 ns       566044 ns         1240
# BM_UuidFromString/32768     4550618 ns      4550611 ns          153
# BM_UuidFromString/262144   36711956 ns     36709939 ns           19
# BM_UuidFromString/524288   72041607 ns     72041217 ns           10
cc_test(
    name = "uuid_benchmark_test",
    size = "large",
    srcs = ["uuid_benchmark_test.cc"],
    args = [
        "--benchmark_counters_tabular=true",
    ],
    copts = BENCHMARK_COPT,
    linkopts = [
        "-lprofiler",
    ],
    tags = ["manual"],
    deps = [
        "//cc/core/common/uuid/src:uuid_lib",
        "@google_benchmark//:benchmark",
        "@gperftools",
    ],
)
