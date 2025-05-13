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

cc_test(
    name = "core_utils_test",
    size = "small",
    srcs = [
        "base64_test.cc",
        "http_test.cc",
    ],
    deps = [
        "//cc/core/interface:interface_lib",
        "//cc/core/utils/src:core_utils",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

# To run the benchmark tests:
#
#   sudo cpupower frequency-set --governor performance
#   bazel test \
#     -c opt \
#     --dynamic_mode=off \
#     --copt=-gmlt \
#     --cache_test_results=no \
#     --//cc:enable_benchmarking=True \
#     //cc/core/utils/test:http_benchmark_test
#
# Sample results:
#
# Run on (12 X 4388.05 MHz CPU s)
# CPU Caches:
#   L1 Data 32 KiB (x6)
#   L1 Instruction 32 KiB (x6)
#   L2 Unified 1024 KiB (x6)
#   L3 Unified 8448 KiB (x1)
# Load Average: 0.51, 0.44, 0.55
# ---------------------------------------------------------------------
# Benchmark                           Time             CPU   Iterations
# ---------------------------------------------------------------------
# BM_ExtractValidUserAgent          178 ns          178 ns      4006728
# BM_ExtractInvalidUserAgent       13.9 ns         13.9 ns     51519221
# ================================================================================
cc_test(
    name = "http_benchmark_test",
    size = "large",
    srcs = ["http_benchmark_test.cc"],
    args = [
        "--benchmark_counters_tabular=true",
    ],
    copts = BENCHMARK_COPT,
    linkopts = [
        "-lprofiler",
    ],
    tags = ["manual"],
    deps = [
        "//cc/core/utils/src:core_utils",
        "@google_benchmark//:benchmark",
        "@gperftools",
    ],
)
