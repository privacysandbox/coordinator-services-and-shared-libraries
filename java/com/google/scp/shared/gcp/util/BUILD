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

load("@rules_java//java:defs.bzl", "java_library")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "util",
    srcs = glob(["*.java"]),
    deps = [
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/mapper",
        "//java/com/google/scp/shared/otel",
        "//java/external:apache_httpclient",
        "//java/external:apache_httpcore",
        "//java/external:apache_httpcore5",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:gcp_cloud_function",
        "//java/external:gcp_cloud_monitoring",
        "//java/external:gcp_monitoring_v3",
        "//java/external:google_auth_library_oauth2_http",
        "//java/external:google_http_client",
        "//java/external:google_http_client_gson",
        "//java/external:google_oauth_client",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:opentelemetry_api",
        "//java/external:opentelemetry_semconv",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
        "//shared/protos/api/v1:java_proto",
    ],
)
