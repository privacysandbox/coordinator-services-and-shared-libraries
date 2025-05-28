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

package(default_visibility = ["//cc:pbs_visibility"])

cc_library(
    name = "error_codes",
    hdrs = ["error_codes.h"],
    deps = [
        "//cc/core/interface:interface_lib",
    ],
)

cc_library(
    name = "http_client_def",
    hdrs = ["http_client_def.h"],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "http_options",
    hdrs = ["http_options.h"],
    deps = [
        "//cc/core/common/operation_dispatcher/src:operation_dispatcher_lib",
    ],
)

cc_library(
    name = "http2_connection",
    srcs = ["http_connection.cc"],
    hdrs = ["http_connection.h"],
    deps = [
        ":error_codes",
        ":http_client_def",
        "//cc/core/common/concurrent_map/src:concurrent_map_lib",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/telemetry/src/metric:telemetry_metric",
        "//cc/core/utils/src:core_utils",
        "@boost//:asio_ssl",
        "@boost//:system",
        "@com_github_nghttp2_nghttp2//:nghttp2_asio",
        "@io_opentelemetry_cpp//sdk/src/metrics",
    ],
)

cc_library(
    name = "http2_connection_pool",
    srcs = ["http_connection_pool.cc"],
    hdrs = ["http_connection_pool.h"],
    deps = [
        ":http2_connection",
        "//cc/core/common/concurrent_map/src:concurrent_map_lib",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/telemetry/src/metric:telemetry_metric",
        "@boost//:system",
        "@com_github_nghttp2_nghttp2//:nghttp2_asio",
        "@io_opentelemetry_cpp//sdk/src/metrics",
    ],
)

cc_library(
    name = "http2_client_lib",
    srcs = ["http2_client.cc"],
    hdrs = ["http2_client.h"],
    deps = [
        ":http2_connection_pool",
        ":http_options",
        "//cc/core/common/operation_dispatcher/src:operation_dispatcher_lib",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/telemetry/src/metric:telemetry_metric",
        "@io_opentelemetry_cpp//sdk/src/metrics",
    ],
)

cc_library(
    name = "synchronous_http2_client",
    srcs = ["synchronous_http2_client.cc"],
    hdrs = ["synchronous_http2_client.h"],
    deps = [
        ":http2_connection_pool",
        ":http_options",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "@com_google_absl//absl/log:check",
    ],
)
