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

load("@rules_cc//cc:defs.bzl", "cc_library")
load("//build_defs/cc:benchmark.bzl", "BENCHMARK_COPT")

package(default_visibility = ["//cc:pbs_visibility"])

cc_library(
    name = "core_async_executor_lib",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
    ),
    copts = BENCHMARK_COPT,
    deps = [
        "//cc/core/common/concurrent_queue/src:concurrent_queue_lib",
        "//cc/core/common/global_logger/src:global_logger_lib",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/test:core_test_lib",
    ],
)
