# Copyright 2023 Google LLC
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
    name = "pbs_gcp_cloud_platform_dependency_factory_lib",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
    ),
    deps = [
        "//cc/core/async_executor/src:core_async_executor_lib",
        "//cc/core/authorization_service/src:core_authorization_service",
        "//cc/core/common/global_logger/src:global_logger_lib",
        "//cc/core/config_provider/src:config_provider_lib",
        "//cc/core/interface:interface_lib",
        "//cc/core/telemetry/src/authentication:telemetry_authentication",
        "//cc/core/telemetry/src/common:telemetry_common",
        "//cc/core/telemetry/src/metric:telemetry_metric",
        "//cc/pbs/authorization/src/aws:aws_http_request_response_auth_interceptor",
        "//cc/pbs/authorization/src/gcp:gcp_http_request_response_auth_interceptor",
        "//cc/pbs/consume_budget/src/gcp:consume_budget",
        "//cc/pbs/interface:pbs_interface_lib",
        "@com_github_googleapis_google_cloud_cpp//:monitoring",
        "@com_github_googleapis_google_cloud_cpp//:opentelemetry",
        "@com_google_absl//absl/base:nullability",
        "@io_opentelemetry_cpp//exporters/otlp:otlp_grpc_metric_exporter",
        "@io_opentelemetry_cpp//sdk/src/resource",
    ],
)
