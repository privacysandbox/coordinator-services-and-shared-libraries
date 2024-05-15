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
    name = "telemetry_authentication",
    srcs = [
        "aws_token_fetcher.cc",
        "gcp_token_fetcher.cc",
        "grpc_auth_config.cc",
        "grpc_id_token_authenticator.cc",
        "token_fetcher_utils.cc",
    ],
    hdrs = [
        "aws_token_fetcher.h",
        "error_codes.h",
        "gcp_token_fetcher.h",
        "grpc_auth_config.h",
        "grpc_id_token_authenticator.h",
        "token_fetcher.h",
        "token_fetcher_utils.h",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/common/concurrent_queue/src:concurrent_queue_lib",
        "//cc/core/common/global_logger/src:global_logger_lib",
        "//cc/core/interface:async_context_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/telemetry/src/common:telemetry_common",
        "//cc/core/test:core_test_lib",
        "//cc/core/utils/src:core_utils",
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
        "@io_opentelemetry_cpp//exporters/otlp:otlp_grpc_metric_exporter",
        "@io_opentelemetry_cpp//ext:headers",
        "@io_opentelemetry_cpp//sdk/src/metrics",
    ],
)
