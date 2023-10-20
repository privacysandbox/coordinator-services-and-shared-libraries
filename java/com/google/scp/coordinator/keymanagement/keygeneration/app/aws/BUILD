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
        "Annotations.java",
        "SqsKeyGenerationQueue.java",
        "SqsKeyGenerationService.java",
    ],
    runtime_deps = [
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
    ],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/aws/model",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/common",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//java/com/google/scp/shared/clients/configclient:aws",
        "//java/external:aws_apache_http",
        "//java/external:aws_auth",
        "//java/external:aws_core",
        "//java/external:aws_regions",
        "//java/external:aws_sqs",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:slf4j",
    ],
)

java_library(
    name = "keyrotation",
    srcs = [
        "KeyRotationLambda.java",
    ],
    runtime_deps = [
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
    ],
    deps = [
        "//java/com/google/crypto/tink/integration/awskmsv2",
        "//java/com/google/scp/shared/util",
        "//java/external:aws_auth",
        "//java/external:aws_dynamodb",
        "//java/external:aws_http_client_spi",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:aws_regions",
        "//java/external:aws_url_connection_client",
        "//java/external:tink",
    ],
)

java_binary(
    name = "KeyRotationLambda",
    runtime_deps = [
        ":keyrotation",
    ],
)

alias(
    name = "KeyRotationLambdaDeploy",
    actual = ":KeyRotationLambda_deploy.jar",
)

pkg_files(
    name = "KeyRotationLambdaDeployPackage",
    srcs = [
        ":KeyRotationLambdaDeploy",
    ],
    prefix = "jars",
    strip_prefix = ".",
)

java_library(
    name = "keygeneration",
    srcs = [
        "AwsKeyGenerationModule.java",
        "KeyGenerationLambda.java",
    ],
    runtime_deps = [
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
    ],
    deps = [
        "//java/com/google/crypto/tink/integration/awskmsv2",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/common",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/aws",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/util",
        "//java/external:aws_auth",
        "//java/external:aws_dynamodb",
        "//java/external:aws_http_client_spi",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:aws_regions",
        "//java/external:aws_url_connection_client",
        "//java/external:guice",
        "//java/external:tink",
    ],
)

java_binary(
    name = "KeyGenerationLambda",
    runtime_deps = [
        ":keygeneration",
    ],
)

alias(
    name = "KeyGenerationLambdaDeploy",
    actual = ":KeyGenerationLambda_deploy.jar",
)

pkg_files(
    name = "KeyGenerationLambdaDeployPackage",
    srcs = [
        ":KeyGenerationLambdaDeploy",
    ],
    prefix = "jars",
    strip_prefix = ".",
)

# Split Key

java_library(
    name = "split_key_generation",
    srcs = [
        "AwsSplitKeyGenerationApplication.java",
        "AwsSplitKeyGenerationArgs.java",
        "AwsSplitKeyGenerationModule.java",
    ],
    runtime_deps = [
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
    ],
    deps = [
        "//java/com/google/crypto/tink/integration/awskmsv2",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/aws",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/app/common",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/aws",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common",
        "//java/com/google/scp/coordinator/keymanagement/keygeneration/tasks/common/keyid",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/aws",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/aws/credsprovider",
        "//java/com/google/scp/shared/aws/util",
        "//java/com/google/scp/shared/clients",
        "//java/com/google/scp/shared/clients/configclient:aws",
        "//java/com/google/scp/shared/crypto/tink",
        "//java/com/google/scp/shared/crypto/tink/aws",
        "//java/external:apache_httpclient",
        "//java/external:aws_apache_http",
        "//java/external:aws_auth",
        "//java/external:aws_core",
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
        "//java/external:aws_http_client_spi",
        "//java/external:aws_kms",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:aws_regions",
        "//java/external:aws_sqs",
        "//java/external:aws_sts",
        "//java/external:aws_url_connection_client",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jcommander",
        "//java/external:tink",
    ],
)

java_binary(
    name = "SplitKeyGenerationApp",
    main_class = "com.google.scp.coordinator.keymanagement.keygeneration.app.aws.AwsSplitKeyGenerationApplication",
    runtime_deps = [
        ":split_key_generation",
    ],
)

alias(
    name = "SplitKeyGenerationAppDeploy",
    actual = ":SplitKeyGenerationApp_deploy.jar",
)

pkg_files(
    name = "SplitKeyGenerationAppDeployPackage",
    srcs = [
        ":SplitKeyGenerationAppDeploy",
    ],
    prefix = "jars",
    strip_prefix = ".",
)
