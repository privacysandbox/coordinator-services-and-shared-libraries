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

package(default_visibility = ["//visibility:public"])

java_library(
    name = "testing",
    srcs = [
        "APIGatewayProxyRequestEventFakeFactory.java",
        "FakeClock.java",
        "FakeEnvironmentVariablesProvider.java",
        "StringToByteSourceConverter.java",
    ],
    deps = [
        "//java/com/google/scp/operator/shared/environment",
        "//java/com/google/scp/shared/mapper",
        "//java/external:aws_lambda_java_events",
        "//java/external:guava",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
    ],
)

java_library(
    name = "test_base_aws_change_handler_module",
    testonly = True,
    srcs = ["TestBaseAwsChangeHandlerModule.java"],
    deps = [
        "//java/com/google/scp/operator/frontend/injection/modules",
        "//java/com/google/scp/operator/frontend/service/aws/changehandler",
        "//java/com/google/scp/operator/frontend/testing:fake_jobmetadata_change_handler",
        "//java/external:autoservice",
        "//java/external:autoservice_annotations",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:guava",
        "//java/external:guice",
    ],
)

java_library(
    name = "dynamo_streams_integration_test_module",
    srcs = [
        "Copy.java",
        "CopyJobHandler.java",
        "DynamoModule.java",
        "DynamoStreamsIntegrationTestModule.java",
        "Source.java",
    ],
    deps = [
        ":common_test_module",
        "//java/com/google/scp/operator/frontend/service/aws",
        "//java/com/google/scp/operator/frontend/service/aws/changehandler",
        "//java/com/google/scp/operator/shared/dao/metadatadb/aws",
        "//java/com/google/scp/operator/shared/injection/modules",
        "//java/com/google/scp/operator/shared/model",
        "//java/external:autoservice",
        "//java/external:autoservice_annotations",
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:aws_regions",
        "//java/external:guava",
        "//java/external:guice",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/metadatadb:java_proto",
    ],
)

java_library(
    name = "integration_test_data_module",
    srcs = ["IntegrationTestDataModule.java"],
    deps = [
        ":testing",
        "//java/com/google/scp/operator/shared/dao/metadatadb/aws",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/com/google/scp/operator/shared/injection/modules",
        "//java/external:autoservice",
        "//java/external:autoservice_annotations",
        "//java/external:guice",
    ],
)

java_binary(
    name = "integration_test_data_module_contained",
    runtime_deps = [":integration_test_data_module"],
)

java_library(
    name = "common_test_module",
    srcs = [
        "CommonTestModule.java",
    ],
    deps = [
        ":testing",
        "//java/external:guice",
    ],
)
