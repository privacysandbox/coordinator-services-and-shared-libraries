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

load("@rules_java//java:defs.bzl", "java_binary", "java_library")
load("@rules_pkg//:pkg.bzl", "pkg_zip")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "gcp",
    srcs = glob([
        "*.java",
    ]),
    deps = [
        "//cc/public/cpio/proto/job_service/v1:job_service_java_proto",
        "//java/com/google/scp/operator/frontend/service",
        "//java/com/google/scp/operator/frontend/service/converter",
        "//java/com/google/scp/operator/frontend/service/model",
        "//java/com/google/scp/operator/frontend/tasks",
        "//java/com/google/scp/operator/frontend/tasks/gcp",
        "//java/com/google/scp/operator/shared/dao/jobqueue/gcp",
        "//java/com/google/scp/operator/shared/dao/metadatadb/gcp",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/gcp/util",
        "//java/com/google/scp/shared/mapper",
        "//java/external:gcp_cloud_function",
        "//java/external:google_api_client",
        "//java/external:google_http_client_gson",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
        "//operator/protos/frontend/api/v1:java_proto",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/metadatadb:java_proto",
    ],
)

java_binary(
    name = "FrontendServiceHttpCloudFunction",
    runtime_deps = [
        ":gcp",
    ],
)

alias(
    name = "FrontendServiceHttpCloudFunctionDeploy",
    actual = ":FrontendServiceHttpCloudFunction_deploy.jar",
)

pkg_zip(
    name = "FrontendServiceHttpCloudFunctionDeploy_zip",
    srcs = [
        ":FrontendServiceHttpCloudFunction_deploy.jar",
    ],
)
