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

load("@rules_java//java:defs.bzl", "java_binary", "java_test")

package(default_visibility = ["//visibility:public"])

java_test(
    name = "DynamoStreamsJobMetadataHandlerTest",
    srcs = ["DynamoStreamsJobMetadataHandlerTest.java"],
    deps = [
        "//java/com/google/scp/operator/frontend/service",
        "//java/com/google/scp/operator/frontend/service/aws",
        "//java/com/google/scp/operator/frontend/service/model",
        "//java/com/google/scp/operator/frontend/testing:fake_jobmetadata_change_handler",
        "//java/com/google/scp/operator/shared/dao/metadatadb/testing:job_generator",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/operator/shared/testing:dynamo_streams_integration_test_module",
        "//java/com/google/scp/operator/shared/testing:test_base_aws_change_handler_module",
        "//java/external:acai",
        "//java/external:aws_lambda_java_events",
        "//java/external:google_truth",
        "//java/external:guice",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/metadatadb:java_proto",
    ],
)

java_test(
    name = "DynamoStreamsJobMetadataHandlerIntegrationTest",
    srcs = ["DynamoStreamsJobMetadataHandlerIntegrationTest.java"],
    tags = ["manual"],
    deps = [
        "//java/com/google/scp/operator/frontend/service",
        "//java/com/google/scp/operator/frontend/service/aws",
        "//java/com/google/scp/operator/shared/dao/metadatadb/aws",
        "//java/com/google/scp/operator/shared/dao/metadatadb/aws/model/converter",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/com/google/scp/operator/shared/dao/metadatadb/testing:job_generator",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/operator/shared/testing",
        "//java/external:acai",
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:aws_regions",
        "//java/external:google_truth",
        "//java/external:guice",
    ],
)

java_test(
    name = "CreateJobApiGatewayHandlerTest",
    srcs = ["CreateJobApiGatewayHandlerTest.java"],
    data = [
        ":resources/invalid_argument.json",
        ":resources/invalid_required_property.json",
        ":resources/missing_required_property.json",
        ":resources/null_required_property.json",
    ],
    deps = [
        "//java/com/google/scp/operator/frontend/injection/factories",
        "//java/com/google/scp/operator/frontend/injection/modules/testing:fake_frontend_module",
        "//java/com/google/scp/operator/frontend/service",
        "//java/com/google/scp/operator/frontend/service/aws",
        "//java/com/google/scp/operator/frontend/service/model",
        "//java/com/google/scp/operator/frontend/tasks",
        "//java/com/google/scp/operator/frontend/testing",
        "//java/com/google/scp/operator/frontend/testing:fake_request_info_validator",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/com/google/scp/operator/shared/dao/metadatadb/testing:fake_metadata_db",
        "//java/com/google/scp/operator/shared/dao/metadatadb/testing:job_generator",
        "//java/com/google/scp/operator/shared/injection/factories",
        "//java/com/google/scp/operator/shared/injection/modules:aws_clients_module",
        "//java/com/google/scp/operator/shared/injection/modules/testing:fake_data_module",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/operator/shared/testing",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/mapper",
        "//java/external:aws_lambda_java_events",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//javatests/com/google/scp/operator/frontend/service",
        "//operator/protos/frontend/api/v1:java_proto",
        "//shared/protos/api/v1:java_proto",
    ],
)

java_test(
    name = "GetJobApiGatewayHandlerTest",
    srcs = ["GetJobApiGatewayHandlerTest.java"],
    deps = [
        "//java/com/google/scp/operator/frontend/injection/factories",
        "//java/com/google/scp/operator/frontend/injection/modules/testing:fake_frontend_module",
        "//java/com/google/scp/operator/frontend/service",
        "//java/com/google/scp/operator/frontend/service/aws",
        "//java/com/google/scp/operator/frontend/service/model",
        "//java/com/google/scp/operator/frontend/tasks",
        "//java/com/google/scp/operator/frontend/testing",
        "//java/com/google/scp/operator/frontend/testing:fake_request_info_validator",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/com/google/scp/operator/shared/dao/metadatadb/testing:fake_metadata_db",
        "//java/com/google/scp/operator/shared/dao/metadatadb/testing:job_generator",
        "//java/com/google/scp/operator/shared/injection/factories",
        "//java/com/google/scp/operator/shared/injection/modules:aws_clients_module",
        "//java/com/google/scp/operator/shared/injection/modules/testing:fake_data_module",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/operator/shared/testing",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/mapper",
        "//java/external:aws_lambda_java_events",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//javatests/com/google/scp/operator/frontend/service",
        "//operator/protos/frontend/api/v1:java_proto",
        "//operator/protos/shared/backend/metadatadb:java_proto",
        "//shared/protos/api/v1:java_proto",
    ],
)

java_test(
    name = "CreateJobIntegrationTest",
    srcs = ["CreateJobIntegrationTest.java"],
    runtime_deps = [
        # LocalStackContainers uses v1 API in the background for auth.
        # However, we don't interface with the v1 API.
        "//java/external:aws_core_java_sdk1",
    ],
    deps = [
        "//java/com/google/scp/operator/frontend/service",
        "//java/com/google/scp/operator/frontend/service/aws",
        "//java/com/google/scp/operator/frontend/service/model",
        "//java/com/google/scp/operator/shared/dao/metadatadb/aws",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/com/google/scp/operator/shared/dao/metadatadb/testing:job_generator",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/operator/shared/testing",
        "//java/com/google/scp/shared/mapper",
        "//java/external:acai",
        "//java/external:aws_core",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:aws_lambda_v2",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:jackson_datatype_jsr310",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//javatests/com/google/scp/operator/frontend/service/aws/testing",
        "//javatests/com/google/scp/shared/testutils/aws",
        "//operator/protos/frontend/api/v1:java_proto",
        "//operator/protos/shared/backend/metadatadb:java_proto",
    ],
)

java_test(
    name = "GetJobIntegrationTest",
    srcs = ["GetJobIntegrationTest.java"],
    runtime_deps = [
        # LocalStackContainers uses v1 API in the background for auth.
        # However, we don't interface with the v1 API.
        "//java/external:aws_core_java_sdk1",
    ],
    deps = [
        "//java/com/google/scp/operator/frontend/service",
        "//java/com/google/scp/operator/frontend/service/aws",
        "//java/com/google/scp/operator/frontend/service/model",
        "//java/com/google/scp/operator/frontend/tasks",
        "//java/com/google/scp/operator/shared/dao/metadatadb/aws",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/com/google/scp/operator/shared/dao/metadatadb/testing:job_generator",
        "//java/com/google/scp/operator/shared/testing",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/mapper",
        "//java/external:acai",
        "//java/external:aws_core",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:aws_lambda_v2",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:jackson_datatype_jsr310",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//javatests/com/google/scp/operator/frontend/service/aws/testing",
        "//javatests/com/google/scp/shared/testutils/aws",
        "//operator/protos/frontend/api/v1:java_proto",
        "//operator/protos/shared/backend/metadatadb:java_proto",
        "//shared/protos/api/v1:java_proto",
    ],
)

java_test(
    name = "DynamoStreamsJobMetadataUpdateCheckerTest",
    srcs = ["DynamoStreamsJobMetadataUpdateCheckerTest.java"],
    deps = [
        "//java/com/google/scp/operator/frontend/service/aws:aws_helpers",
        "//java/com/google/scp/operator/shared/dao/metadatadb/testing:job_generator",
        "//java/com/google/scp/operator/shared/model",
        "//java/external:acai",
        "//java/external:aws_lambda_java_events",
        "//java/external:google_truth8",
        "//java/external:guice",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/metadatadb:java_proto",
    ],
)

java_test(
    name = "DdbStreamJobMetadataLookupTest",
    srcs = ["DdbStreamJobMetadataLookupTest.java"],
    # Test passes locally but fails in kokoro, with a JobKeyExistsException thrown in setUp
    tags = ["manual"],
    runtime_deps = [
        # LocalStackContainers uses v1 API in the background for auth.
        # However, we don't interface with the v1 API.
        # TODO: Find a way to not have to copy this to all other AWS tests.
        "//java/external:aws_core_java_sdk1",
    ],
    deps = [
        "//java/com/google/scp/operator/frontend/service/aws:aws_helpers",
        "//java/com/google/scp/operator/frontend/service/model",
        "//java/external:acai",
        "//java/external:autoservice",
        "//java/external:autoservice_annotations",
        "//java/external:aws_apache_http",
        "//java/external:aws_auth",
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:localstack_containers",
        "//javatests/com/google/scp/operator/shared/testing:hermetic_aws_clients_module",
        "//javatests/com/google/scp/shared/testutils/aws",
    ],
)

java_test(
    name = "DdbStreamBatchInfoParserTest",
    srcs = ["DdbStreamBatchInfoParserTest.java"],
    data = [
        ":resources/failed_stream_event_invalid_missing_shard_id.json",
        ":resources/failed_stream_event_invalid_missing_stream_batch_info.json",
        ":resources/failed_stream_event_valid.json",
    ],
    deps = [
        "//java/com/google/scp/operator/frontend/service/aws:aws_helpers",
        "//java/com/google/scp/operator/frontend/service/aws/model",
        "//java/external:acai",
        "//java/external:google_truth",
        "//java/external:guice",
    ],
)

java_test(
    name = "AwsFailedJobQueueWriteCleanupTest",
    srcs = ["AwsFailedJobQueueWriteCleanupTest.java"],
    data = [
        ":AwsFailedJobQueueWriteCleanupTestLambdaDeploy",
    ],
    env = {
        "LAMBDA_LOCATION": "$(location :AwsFailedJobQueueWriteCleanupTestLambdaDeploy)",
    },
    runtime_deps = [
        # LocalStackContainers uses v1 API in the background for auth.
        # However, we don't interface with the v1 API.
        # TODO: Find a way to not have to copy this to all other AWS tests.
        "//java/external:aws_core_java_sdk1",
    ],
    deps = [
        "//java/com/google/scp/operator/frontend/service/aws/changehandler",
        "//java/com/google/scp/operator/frontend/service/aws/model",
        "//java/com/google/scp/operator/shared/dao/metadatadb/aws",
        "//java/com/google/scp/operator/shared/dao/metadatadb/testing:job_generator",
        "//java/com/google/scp/operator/shared/injection/modules",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/operator/shared/testing:integration_test_data_module",
        "//java/com/google/scp/shared/proto",
        "//java/external:acai",
        "//java/external:autoservice",
        "//java/external:autoservice_annotations",
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
        "//java/external:aws_lambda_v2",
        "//java/external:aws_s3",
        "//java/external:aws_sqs",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:localstack_containers",
        "//javatests/com/google/scp/operator/shared/testing:hermetic_aws_clients_module",
        "//javatests/com/google/scp/shared/testutils/aws",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/metadatadb:java_proto",
    ],
)

java_binary(
    name = "AwsFailedJobQueueWriteCleanupTestLambda",
    runtime_deps = [
        "//java/com/google/scp/operator/frontend/service/aws",
        "//java/com/google/scp/operator/frontend/testing:aws_failed_job_queue_write_cleanup_test_module",
        "//java/com/google/scp/operator/shared/injection/modules:aws_clients_module",
        "//java/com/google/scp/operator/shared/testing:integration_test_data_module",
        "//java/external:commons_logging",
    ],
)

# The JAR that will run the lambda handler in AwsFailedJobQueueWriteCleanupTest
alias(
    name = "AwsFailedJobQueueWriteCleanupTestLambdaDeploy",
    actual = ":AwsFailedJobQueueWriteCleanupTestLambda_deploy.jar",
)
