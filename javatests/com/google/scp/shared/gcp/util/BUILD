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

load("@rules_java//java:defs.bzl", "java_test")

package(default_visibility = ["//visibility:public"])

java_test(
    name = "GcpHttpInterceptorUtilTest",
    srcs = ["GcpHttpInterceptorUtilTest.java"],
    deps = [
        "//java/com/google/scp/shared/gcp/util",
        "//java/external:apache_httpcore",
        "//java/external:google_auth_library_oauth2_http",
        "//java/external:google_truth",
        "//java/external:junit",
        "//java/external:mockito",
    ],
)

java_test(
    name = "CloudFunctionUtilTest",
    srcs = ["CloudFunctionUtilTest.java"],
    deps = [
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/gcp/util",
        "//java/com/google/scp/shared/mapper",
        "//java/external:gcp_cloud_function",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:jackson_databind",
        "//java/external:mockito",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//javatests/com/google/scp/coordinator/keymanagement/testutils",
        "//shared/protos/api/v1:java_proto",
    ],
)

java_test(
    name = "CloudFunctionServiceBaseTest",
    srcs = ["CloudFunctionServiceBaseTest.java"],
    deps = [
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/gcp/util",
        "//java/external:assertj_core",
        "//java/external:gcp_cloud_function",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:jackson_databind",
        "//java/external:mockito",
        "//java/external:opentelemetry_api",
        "//java/external:opentelemetry_sdk_common",
        "//java/external:opentelemetry_sdk_metrics",
        "//java/external:opentelemetry_sdk_testing",
        "//java/external:opentelemetry_semconv",
    ],
)
