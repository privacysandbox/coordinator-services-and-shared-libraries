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

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "interface_lib",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
        exclude = [
            "errors.cc",
            "errors.h",
            "async_context.h",
            "type_def.h",
            "logger_interface.h",
            "service_interface.h",
        ],
    ),
    copts = BENCHMARK_COPT,
    deps = [
        ":async_context_lib",
        ":service_interface_lib",
        "//cc/core/common/concurrent_map/src:concurrent_map_lib",
        "//cc/core/common/proto:core_common_proto_lib",
        "//cc/core/common/uuid/src:uuid_lib",
    ],
)

cc_library(
    name = "service_interface_lib",
    srcs = [
        "service_interface.h",
    ],
    deps = [
    ],
)

cc_library(
    name = "logger_interface",
    srcs =
        [
            "logger_interface.h",
        ],
    deps = [
        ":service_interface_lib",
        "//cc/core/common/uuid/src:uuid_lib",
    ],
)

cc_library(
    name = "async_context_lib",
    srcs =
        [
            "async_context.h",
        ],
    deps = [
        "//cc/core/common/global_logger/src:global_logger_lib",
        "//cc/core/common/uuid/src:uuid_lib",
    ],
)

cc_library(
    name = "type_def_lib",
    srcs =
        [
            "type_def.h",
        ],
    deps = [
        "//cc/core/common/proto:core_common_proto_lib",
        "@com_google_absl//absl/base:core_headers",
    ],
)

cc_library(
    name = "errors_lib",
    srcs =
        [
            "errors.cc",
            "errors.h",
        ],
    deps = [
        "//cc/public/core/interface:execution_result",
    ],
)
