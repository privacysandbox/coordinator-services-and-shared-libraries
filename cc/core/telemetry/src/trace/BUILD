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

package(default_visibility = ["//cc:pbs_visibility"])

cc_library(
    name = "trace",
    srcs = [
        "trace_router.cc",
        "trace_sampler.cc",
    ],
    hdrs = [
        "error_codes.h",
        "trace_router.h",
        "trace_sampler.h",
    ],
    deps = [
        "//cc/core/common/concurrent_queue/src:concurrent_queue_lib",
        "//cc/core/common/global_logger/src:global_logger_lib",
        "//cc/core/telemetry/src/common:telemetry_common",
        "@com_github_googleapis_google_cloud_cpp//:iam",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@io_opentelemetry_cpp//api",
        "@io_opentelemetry_cpp//exporters/otlp:otlp_grpc_exporter",
        "@io_opentelemetry_cpp//ext:headers",
        "@io_opentelemetry_cpp//sdk/src/resource",
        "@io_opentelemetry_cpp//sdk/src/trace",
    ],
)
