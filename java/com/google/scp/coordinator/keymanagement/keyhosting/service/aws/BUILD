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
    name = "AwsKeyServiceModule",
    srcs = [
        "AwsKeyServiceModule.java",
    ],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/common",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/tasks",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/aws",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/external:aws_apache_http",
        "//java/external:aws_auth",
        "//java/external:aws_http_client_spi",
        "//java/external:aws_url_connection_client",
        "//java/external:guice",
    ],
)

java_library(
    name = "PublicKeyApiGatewayHandler",
    srcs = [
        "PublicKeyApiGatewayHandler.java",
    ],
    runtime_deps = [
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
    ],
    deps = [
        ":AwsKeyServiceModule",
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/common",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/aws/util",
        "//java/com/google/scp/shared/mapper",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
    ],
)

java_binary(
    name = "PublicKeyApiGatewayHandlerLambda",
    runtime_deps = [
        ":PublicKeyApiGatewayHandler",
    ],
)

alias(
    name = "PublicKeyApiGatewayHandlerDeploy",
    actual = ":PublicKeyApiGatewayHandlerLambda_deploy.jar",
)

pkg_files(
    name = "PublicKeyApiGatewayHandlerDeployPackage",
    srcs = [
        ":PublicKeyApiGatewayHandlerDeploy",
    ],
    prefix = "jars",
    strip_prefix = ".",
)

java_library(
    name = "GetEncryptionKeyApiGatewayHandler",
    srcs = [
        "GetEncryptionKeyApiGatewayHandler.java",
    ],
    deps = [
        ":AwsKeyServiceModule",
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/api/v1:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/common",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/common/converter",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/aws/util",
        "//java/com/google/scp/shared/mapper",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:guava",
        "//java/external:guice",
    ],
)

java_library(
    name = "ListRecentEncryptionKeysApiGatewayHandler",
    srcs = [
        "ListRecentEncryptionKeysApiGatewayHandler.java",
    ],
    deps = [
        ":AwsKeyServiceModule",
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/api/v1:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/aws/util",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:guava",
        "//java/external:guice",
    ],
)

java_binary(
    name = "EncryptionKeyServiceLambda",
    runtime_deps = [
        ":GetEncryptionKeyApiGatewayHandler",
        ":ListRecentEncryptionKeysApiGatewayHandler",
    ],
)

alias(
    name = "EncryptionKeyServiceLambdaDeploy",
    actual = ":EncryptionKeyServiceLambda_deploy.jar",
)
