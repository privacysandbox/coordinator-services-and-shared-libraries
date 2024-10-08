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
load("@rules_java//java:defs.bzl", "java_library")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "otel",
    srcs = glob(["*.java"]),
    deps = [
        "//java/external:gcp_cloud_monitoring",
        "//java/external:gcp_exporter_metrics",
        "//java/external:gcp_monitoring_v3",
        "//java/external:google_auth_library_oauth2_http",
        "//java/external:google_common_proto",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:opentelemetry_api",
        "//java/external:opentelemetry_contrib_gcp_resources",
        "//java/external:opentelemetry_sdk",
        "//java/external:opentelemetry_sdk_common",
        "//java/external:opentelemetry_sdk_extension_autoconfigure_spi",
        "//java/external:opentelemetry_sdk_metrics",
        "//java/external:slf4j",
    ],
)
