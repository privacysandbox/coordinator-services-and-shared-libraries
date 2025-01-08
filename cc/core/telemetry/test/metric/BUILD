# Copyright 2024 Google LLC
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
    name = "telemetry_metric_integration_test",
    size = "small",
    srcs = ["metric_router_in_memory_integration_test.cc"],
    deps = [
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/telemetry/mock:telemetry_fake",
        "//cc/core/telemetry/src/common:telemetry_common",
        "//cc/core/telemetry/src/metric:telemetry_metric",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@io_opentelemetry_cpp//api",
        "@io_opentelemetry_cpp//exporters/otlp:otlp_grpc_exporter",
        "@io_opentelemetry_cpp//exporters/otlp:otlp_grpc_metric_exporter",
        "@io_opentelemetry_cpp//sdk/src/metrics",
    ],
)

cc_test(
    name = "telemetry_metric_tests",
    size = "small",
    srcs = [
        "metric_router_test.cc",
        "otlp_grpc_authed_metric_exporter_test.cc",
    ],
    deps = [
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/telemetry/mock:telemetry_fake",
        "//cc/core/telemetry/src/metric:telemetry_metric",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@io_opentelemetry_cpp//api",
        "@io_opentelemetry_cpp//exporters/otlp:otlp_grpc_exporter",
        "@io_opentelemetry_cpp//exporters/otlp:otlp_grpc_metric_exporter",
        "@io_opentelemetry_cpp//ext:headers",
        "@io_opentelemetry_cpp//sdk/src/metrics",
        "@io_opentelemetry_cpp//sdk/src/resource",
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
#     //cc/core/telemetry/test/metric:metric_benchmark_test
#
# Sample results:
#
# Run on (64 X 2450 MHz CPU s)
# CPU Caches:
#   L1 Data 32 KiB (x32)
#   L1 Instruction 32 KiB (x32)
#   L2 Unified 512 KiB (x32)
#   L3 Unified 32768 KiB (x4)
# Load Average: 5.93, 2.58, 2.01
# ----------------------------------------------------------------------------------
# Benchmark                                        Time             CPU   Iterations
# ----------------------------------------------------------------------------------
# BM_SpinLockIncrement/threads:1                3.12 ns         3.12 ns    224950098
# BM_SpinLockIncrement/threads:2                5.36 ns         7.75 ns    115034540
# BM_SpinLockIncrement/threads:4                8.20 ns         14.7 ns     39651696
# BM_SpinLockIncrement/threads:8                10.4 ns         20.6 ns     31445456
# BM_SpinLockIncrement/threads:16               13.0 ns         30.0 ns     22597824
# BM_SpinLockIncrement/threads:32               20.1 ns         52.0 ns     23550016
# BM_SpinLockIncrement/threads:64               20.6 ns         66.8 ns     13207360
# BM_AtomicIncrement/threads:1                  2.29 ns         2.29 ns    282728638
# BM_AtomicIncrement/threads:2                  5.70 ns         11.4 ns     56843798
# BM_AtomicIncrement/threads:4                  6.78 ns         27.1 ns     33190772
# BM_AtomicIncrement/threads:8                  7.96 ns         63.7 ns      8000000
# BM_AtomicIncrement/threads:16                 13.5 ns          216 ns      5732272
# BM_AtomicIncrement/threads:32                 12.1 ns          374 ns      2521184
# BM_AtomicIncrement/threads:64                 6.97 ns          430 ns      1270272
# BM_OtelCounterIncrement/threads:1             86.1 ns         86.1 ns      8144939
# BM_OtelCounterIncrement/threads:2              103 ns          180 ns      5659808
# BM_OtelCounterIncrement/threads:4             97.9 ns          158 ns      4000000
# BM_OtelCounterIncrement/threads:8             97.3 ns          192 ns      4041040
# BM_OtelCounterIncrement/threads:16             106 ns          215 ns      3085312
# BM_OtelCounterIncrement/threads:32             127 ns          346 ns      2899840
# BM_OtelCounterIncrement/threads:64             138 ns          429 ns      1771904
# BM_OTelCounterAsyncIncrement/threads:1        39.7 ns         39.7 ns     17252919
# BM_OTelCounterAsyncIncrement/threads:2        51.1 ns          102 ns     12398732
# BM_OTelCounterAsyncIncrement/threads:4        46.6 ns          186 ns      3981984
# BM_OTelCounterAsyncIncrement/threads:8        45.0 ns          360 ns      2621712
# BM_OTelCounterAsyncIncrement/threads:16       26.2 ns          419 ns      1758240
# BM_OTelCounterAsyncIncrement/threads:32       20.7 ns          662 ns       983616
# BM_OTelCounterAsyncIncrement/threads:64       15.4 ns          969 ns       988864
# ================================================================================
cc_test(
    name = "metric_benchmark_test",
    size = "large",
    srcs = ["metric_benchmark_test.cc"],
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
        "//cc/core/telemetry/mock:telemetry_fake",
        "@google_benchmark//:benchmark",
        "@gperftools",
    ],
)
