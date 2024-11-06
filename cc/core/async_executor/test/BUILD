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
    name = "async_executor_test",
    size = "small",
    srcs = ["async_executor_test.cc"],
    linkopts = [
        "-lprofiler",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
        "@gperftools",
    ],
)

cc_test(
    name = "single_thread_async_executor_test",
    size = "small",
    srcs = ["single_thread_async_executor_test.cc"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "single_thread_priority_async_executor_test",
    size = "small",
    timeout = "moderate",
    srcs = ["single_thread_priority_async_executor_test.cc"],
    flaky = True,
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "async_executor_benchmark_tests",
    size = "small",
    srcs = [
        "single_thread_async_executor_benchmark_test.cc",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "async_task_test",
    size = "small",
    srcs = ["async_task_test.cc"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/mock:core_async_executor_mock",
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/test/utils:utils_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "async_executor_utils_test",
    size = "small",
    srcs = ["async_executor_utils_test.cc"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/async_executor/src:core_async_executor_lib",
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
#     //cc/core/async_executor/test:async_executor_benchmark_test
#
# Sample results:
#
# Run on (12 X 4371.87 MHz CPU s)
# CPU Caches:
#   L1 Data 32 KiB (x6)
#   L1 Instruction 32 KiB (x6)
#   L2 Unified 1024 KiB (x6)
#   L3 Unified 8448 KiB (x1)
# Load Average: 0.36, 0.49, 0.58
# ------------------------------------------------------------------------------------------------
# Benchmark                                Time             CPU   Iterations        p50        p99
# ------------------------------------------------------------------------------------------------
# ExecutorFixture/Schedule/1           10056 ns         4884 ns       115533          4          7
# ExecutorFixture/Schedule/8           24847 ns        18867 ns        35663          4         18
# ExecutorFixture/Schedule/64         132131 ns       122867 ns         4812          4        109
# ExecutorFixture/Schedule/512        965677 ns       945822 ns          575          4        823
# ExecutorFixture/Schedule/4096      8884932 ns      8037362 ns           67          4     2.827k
# ExecutorFixture/Schedule/32768    69344449 ns     63387042 ns           10          4     3.187k
# ExecutorFixture/Schedule/262144  550289512 ns    498346952 ns            2          4      3.01k
# ExecutorFixture/Schedule/524288 1109173536 ns    990891695 ns            1          4     2.761k
# ================================================================================
cc_test(
    name = "async_executor_benchmark_test",
    size = "large",
    srcs = ["async_executor_benchmark_test.cc"],
    args = [
        "--benchmark_counters_tabular=true",
    ],
    copts = BENCHMARK_COPT,
    linkopts = [
        "-lprofiler",
    ],
    tags = ["manual"],
    deps = [
        "//cc/core/async_executor/src:core_async_executor_lib",
        "@google_benchmark//:benchmark",
        "@gperftools",
    ],
)
