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

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "telemetry_common",
    srcs = [],
    hdrs = ["telemetry_configuration.h"],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "telemetry_metric_utils",
    srcs = [
        "metric_utils.cc",
    ],
    hdrs = [
        "metric_utils.h",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/telemetry/src/common:telemetry_common",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@io_opentelemetry_cpp//exporters/otlp:otlp_grpc_exporter",
        "@io_opentelemetry_cpp//exporters/otlp:otlp_grpc_metric_exporter",
        "@io_opentelemetry_cpp//ext:headers",
        "@io_opentelemetry_cpp//sdk/src/metrics",
    ],
)
