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

package(default_visibility = ["//cc:pbs_visibility"])

cc_test(
    name = "telemetry_mock_tests",
    size = "small",
    srcs = ["in_memory_metric_exporter_test.cc"],
    deps = [
        "//cc/core/config_provider/mock:core_config_provider_mock",
        "//cc/core/telemetry/mock:telemetry_fake",
        "//cc/core/telemetry/src/common:telemetry_common",
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
