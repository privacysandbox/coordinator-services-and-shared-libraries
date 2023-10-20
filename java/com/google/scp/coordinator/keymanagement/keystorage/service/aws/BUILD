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
    name = "AwsKeyStorageServiceModule",
    srcs = [
        "AwsKeyStorageServiceModule.java",
    ],
    deps = [
        "//java/com/google/crypto/tink/integration/awskmsv2",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/service/common",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/tasks/aws",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/aws",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/crypto/tink",
        "//java/com/google/scp/shared/crypto/tink/aws",
        "//java/external:aws_apache_http",
        "//java/external:aws_auth",
        "//java/external:aws_http_client_spi",
        "//java/external:aws_kms",
        "//java/external:aws_regions",
        "//java/external:aws_url_connection_client",
        "//java/external:guice",
        "//java/external:tink",
    ],
)

java_library(
    name = "CreateKeyApiGatewayHandler",
    srcs = [
        "CreateKeyApiGatewayHandler.java",
    ],
    runtime_deps = [
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
    ],
    deps = [
        ":AwsKeyStorageServiceModule",
        "//coordinator/protos/keymanagement/keystorage/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/api/v1:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/service/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/aws/util",
        "//java/com/google/scp/shared/mapper",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
    ],
)

java_library(
    name = "GetDataKeyApiGatewayHandler",
    srcs = [
        "GetDataKeyApiGatewayHandler.java",
    ],
    runtime_deps = [
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
    ],
    deps = [
        ":AwsKeyStorageServiceModule",
        "//coordinator/protos/keymanagement/keystorage/api/v1:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/keystorage/service/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/aws/util",
        "//java/com/google/scp/shared/mapper",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
    ],
)

java_binary(
    name = "KeyStorageServiceLambda",
    runtime_deps = [
        ":CreateKeyApiGatewayHandler",
        ":GetDataKeyApiGatewayHandler",
    ],
)

alias(
    name = "KeyStorageServiceLambdaDeploy",
    actual = ":KeyStorageServiceLambda_deploy.jar",
)
