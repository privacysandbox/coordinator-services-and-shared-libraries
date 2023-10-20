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
    name = "aws",
    srcs = glob(["*.java"]),
    runtime_deps = [
        # LocalStackContainers uses v1 API in the background.
        # However, we don't interface with the v1 API.
        # TODO: consider scoping the localstack libraries in this target into
        # their own target -- at the time of writing at the time of writing this
        # target only contains localstack utils.
        "//java/external:aws_core_java_sdk1",
    ],
    deps = [
        "//java/com/google/crypto/tink/integration/awskmsv2",
        "//java/com/google/scp/shared/crypto/tink",
        "//java/com/google/scp/shared/crypto/tink/aws",
        "//java/external:acai",
        "//java/external:apache_httpclient",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:aws_apache_http",
        "//java/external:aws_api_gateway_v1",
        "//java/external:aws_auth",
        "//java/external:aws_core",
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
        "//java/external:aws_http_client_spi",
        "//java/external:aws_kms",
        "//java/external:aws_lambda_v2",
        "//java/external:aws_netty_nio_client",
        "//java/external:aws_regions",
        "//java/external:aws_s3",
        "//java/external:aws_sqs",
        "//java/external:aws_sts",
        "//java/external:findbugs",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_annotations",
        "//java/external:jackson_databind",
        "//java/external:jcommander",
        "//java/external:localstack_containers",
        "//java/external:slf4j",
        "//java/external:testcontainers",
    ],
)
