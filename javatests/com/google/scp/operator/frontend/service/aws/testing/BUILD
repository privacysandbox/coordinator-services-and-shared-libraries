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
    name = "testing",
    srcs = glob(["*.java"]),
    data = [
        # LINT.IfChange(integration-frontend-deploy-jar)
        "//java/com/google/scp/operator/frontend:AwsApigatewayFrontend_deploy.jar",
        # LINT.ThenChange(LocalFrontendServiceModule.java:integration-frontend-deploy-jar)
    ],
    deps = [
        "//java/com/google/scp/operator/shared/dao/metadatadb/aws",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/com/google/scp/operator/shared/injection/modules",
        "//java/external:acai",
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
        "//java/external:aws_lambda_v2",
        "//java/external:aws_s3",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:localstack_containers",
        "//javatests/com/google/scp/shared/testutils/aws",
    ],
)
