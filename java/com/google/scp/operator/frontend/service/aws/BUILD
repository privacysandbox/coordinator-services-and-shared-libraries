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

load("@rules_java//java:defs.bzl", "java_binary", "java_library")
load("@rules_pkg//:mappings.bzl", "pkg_files")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "aws",
    srcs = [
        "AwsFailedJobQueueWriteCleanup.java",
        "CreateJobApiGatewayHandler.java",
        "DynamoStreamsJobMetadataHandler.java",
        "GetJobApiGatewayHandler.java",
    ],
    deps = [
        ":aws_helpers",
        "//java/com/google/scp/operator/frontend/injection/factories",
        "//java/com/google/scp/operator/frontend/serialization",
        "//java/com/google/scp/operator/frontend/service",
        "//java/com/google/scp/operator/frontend/service/aws/changehandler",
        "//java/com/google/scp/operator/frontend/service/model",
        "//java/com/google/scp/operator/frontend/tasks",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/aws/util",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:guava",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//java/external:slf4j",
        "//operator/protos/frontend/api/v1:java_proto",
    ],
)

java_library(
    name = "aws_helpers",
    srcs = [
        "DdbStreamBatchInfoParser.java",
        "DdbStreamJobMetadataLookup.java",
        "DynamoStreamsJobMetadataUpdateChecker.java",
    ],
    deps = [
        "//java/com/google/scp/operator/frontend/service/aws/model",
        "//java/com/google/scp/operator/shared/dao/metadatadb/aws/model/converter",
        "//java/com/google/scp/operator/shared/model",
        "//java/external:aws_dynamodb",
        "//java/external:aws_lambda_java_events",
        "//java/external:aws_lambda_java_events_sdk_transformer",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//operator/protos/shared/backend/metadatadb:java_proto",
    ],
)

java_binary(
    name = "AwsChangeHandlerLambda",
    runtime_deps = [
        ":aws",
        "//java/com/google/scp/operator/frontend/injection/modules:aws_change_handler_module",
        "//java/external:commons_logging",
    ],
)

# Deploy jar for the change handler (the suffix "_deploy.jar" tells bazel to
# include dependencies)
alias(
    name = "AwsChangeHandlerLambdaDeploy",
    actual = ":AwsChangeHandlerLambda_deploy.jar",
)

pkg_files(
    name = "AwsChangeHandlerLambdaDeployPackage",
    srcs = [
        ":AwsChangeHandlerLambdaDeploy",
    ],
    prefix = "jars",
    strip_prefix = ".",
)

java_binary(
    name = "AwsFrontendCleanupLambda",
    runtime_deps = [
        ":aws",
        "//java/com/google/scp/operator/frontend/injection/modules:aws_failed_job_queue_change_handler_module",
        "//java/com/google/scp/operator/frontend/injection/modules:aws_failed_job_queue_write_cleanup_module",
        "//java/com/google/scp/operator/shared/injection/modules:aws_clients_module",
        "//java/com/google/scp/operator/shared/injection/modules:data_module",
        "//java/external:commons_logging",
    ],
)

alias(
    name = "AwsFrontendCleanupLambdaDeploy",
    actual = "AwsFrontendCleanupLambda_deploy.jar",
)

pkg_files(
    name = "AwsFrontendCleanupLambdaDeployPackage",
    srcs = [
        ":AwsFrontendCleanupLambdaDeploy",
    ],
    prefix = "jars",
    strip_prefix = ".",
)
