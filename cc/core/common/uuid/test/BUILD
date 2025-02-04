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

package(default_visibility = ["//cc:pbs_visibility"])

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

# To run the test:
#   sudo cpupower frequency-set --governor performance
#   bazel test \
#     -c opt \
#     --dynamic_mode=off \
#     --copt=-gmlt \
#     --cache_test_results=no \
#     --//cc:enable_benchmarking=True \
#     //cc/core/common/uuid/test:uuid_benchmark_test
#
# Run on (12 X 4261.69 MHz CPU s)
# CPU Caches:
#   L1 Data 32 KiB (x6)
#   L1 Instruction 32 KiB (x6)
#   L2 Unified 1024 KiB (x6)
#   L3 Unified 8448 KiB (x1)
# Load Average: 1.34, 1.81, 1.15
# -------------------------------------------------------------------
# Benchmark                         Time             CPU   Iterations
# -------------------------------------------------------------------
# BM_UuidFromString/1             116 ns          116 ns      5976647
# BM_UuidFromString/8            1015 ns         1015 ns       718066
# BM_UuidFromString/64           7753 ns         7752 ns        90862
# BM_UuidFromString/512         61760 ns        61759 ns        11329
# BM_UuidFromString/4096       493619 ns       493617 ns         1407
# BM_UuidFromString/32768     3947528 ns      3947020 ns          176
# BM_UuidFromString/262144   32016917 ns     32014937 ns           22
# BM_UuidFromString/524288   63297467 ns     63295117 ns           11
# BM_UuidToString/1              42.4 ns         42.4 ns     16435631
# BM_UuidToString/8               343 ns          343 ns      2038180
# BM_UuidToString/64             2685 ns         2685 ns       260703
# BM_UuidToString/512           21406 ns        21404 ns        32710
# BM_UuidToString/4096         171177 ns       171170 ns         4088
# BM_UuidToString/32768       1374535 ns      1374397 ns          510
# BM_UuidToString/262144     11016479 ns     11015675 ns           63
# BM_UuidToString/524288     21998510 ns     21996346 ns           32
# ================================================================================
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
