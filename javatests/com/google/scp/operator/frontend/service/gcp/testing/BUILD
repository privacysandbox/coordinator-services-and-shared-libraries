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

package(default_visibility = ["//visibility:public"])

java_library(
    name = "LocalFrontendServiceHttpFunction",
    testonly = True,
    srcs = [
        "LocalFrontendServiceHttpFunction.java",
    ],
    deps = [
        ":FrontendServiceTestModule",
        "//java/com/google/scp/operator/frontend/service/gcp",
        "//java/com/google/scp/operator/frontend/service/gcp:FrontendServiceHttpCloudFunction",
        "//java/external:gcp_core",
        "//java/external:gcp_spanner",
        "//java/external:guice",
        "//java/external:testcontainers",
        "//javatests/com/google/scp/operator/shared/dao/metadatadb/gcp",
        "//javatests/com/google/scp/shared/testutils/gcp",
    ],
)

java_binary(
    name = "LocalFrontendServiceHttpCloudFunction",
    testonly = True,
    runtime_deps = [
        ":LocalFrontendServiceHttpFunction",
    ],
)

alias(
    name = "LocalFrontendServiceHttpCloudFunctionDeploy",
    testonly = True,
    actual = ":LocalFrontendServiceHttpCloudFunction_deploy.jar",
)

java_library(
    name = "FrontendServiceTestModule",
    srcs = glob([
        "FrontendServiceTestModule.java",
    ]),
    deps = [
        "//java/com/google/scp/operator/frontend/service",
        "//java/com/google/scp/operator/frontend/service/converter",
        "//java/com/google/scp/operator/frontend/service/gcp",
        "//java/com/google/scp/operator/frontend/service/model",
        "//java/com/google/scp/operator/frontend/tasks",
        "//java/com/google/scp/operator/frontend/tasks/gcp",
        "//java/com/google/scp/operator/shared/dao/jobqueue/common",
        "//java/com/google/scp/operator/shared/dao/jobqueue/gcp",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/com/google/scp/operator/shared/dao/metadatadb/gcp",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/gcp/util",
        "//java/com/google/scp/shared/mapper",
        "//java/external:acai",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:gcp_api_gax",
        "//java/external:gcp_cloud_function",
        "//java/external:gcp_core",
        "//java/external:gcp_gax_grpc",
        "//java/external:gcp_pubsub",
        "//java/external:gcp_pubsub_v1",
        "//java/external:gcp_spanner",
        "//java/external:google_truth",
        "//java/external:grpc_api",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:guice-testlib",
        "//java/external:jackson_annotations",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:javax_inject",
        "//java/external:localstack_containers",
        "//java/external:mockito",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
        "//java/external:testcontainers",
        "//java/external:tink",
        "//java/external:tink-gcpkms",
        "//javatests/com/google/scp/e2e/shared:testutils",
        "//javatests/com/google/scp/operator/shared/dao/jobqueue/gcp",
        "//javatests/com/google/scp/operator/shared/dao/metadatadb/gcp",
        "//javatests/com/google/scp/shared/gcp:constants",
        "//javatests/com/google/scp/shared/testutils/common",
        "//javatests/com/google/scp/shared/testutils/gcp",
        "//operator/protos/frontend/api/v1:java_proto",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/metadatadb:java_proto",
    ],
)

java_library(
    name = "FrontendserviceCloudFunctinEmulatorContainer",
    srcs = ["FrontendserviceCloudFunctinEmulatorContainer.java"],
    runtime_deps = [
        "//java/external:gcp_function_invoker",
        "//javatests/com/google/scp/operator/frontend/service/gcp/testing:LocalFrontendServiceHttpCloudFunctionDeploy",
    ],
    deps = [
        "//java/external:guice",
        "//javatests/com/google/scp/shared/testutils/gcp",
    ],
)

java_library(
    name = "FrontendIntegrationTestEnv",
    srcs = glob([
        "FrontendServiceIntegrationTestEnv.java",
    ]),
    deps = [
        ":FrontendserviceCloudFunctinEmulatorContainer",
        "//java/com/google/scp/operator/shared/dao/jobqueue/common",
        "//java/com/google/scp/operator/shared/dao/jobqueue/gcp",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/com/google/scp/operator/shared/dao/metadatadb/gcp",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/gcp/util",
        "//java/com/google/scp/shared/mapper",
        "//java/external:acai",
        "//java/external:gcp_pubsub",
        "//java/external:gcp_pubsub_v1",
        "//java/external:gcp_spanner",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:guice-testlib",
        "//java/external:jackson_annotations",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:javax_inject",
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
        "//java/external:testcontainers",
        "//java/external:tink",
        "//java/external:tink-gcpkms",
        "//javatests/com/google/scp/e2e/shared:testutils",
        "//javatests/com/google/scp/operator/shared/dao/jobqueue/gcp",
        "//javatests/com/google/scp/operator/shared/dao/metadatadb/gcp",
        "//javatests/com/google/scp/shared/testutils/common",
        "//javatests/com/google/scp/shared/testutils/gcp",
    ],
)
