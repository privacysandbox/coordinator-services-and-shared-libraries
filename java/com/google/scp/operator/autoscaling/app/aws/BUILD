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
    name = "AsgCapacityHandler",
    srcs = [
        "AsgCapacityHandler.java",
        "AsgCapacityModule.java",
    ],
    deps = [
        "//java/com/google/scp/operator/autoscaling/tasks/aws:AutoScalingTasks",
        "//java/com/google/scp/operator/shared/dao/jobqueue/aws",
        "//java/com/google/scp/shared/api/exception",
        "//java/external:aws_auth",
        "//java/external:aws_auto_scaling",
        "//java/external:aws_http_client_spi",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:aws_sqs",
        "//java/external:aws_url_connection_client",
        "//java/external:guice",
    ],
)

alias(
    name = "AsgCapacityHandlerLambdaDeploy",
    actual = ":AsgCapacityHandlerLambda_deploy.jar",
)

pkg_files(
    name = "AsgCapacityHandlerLambdaDeployPackage",
    srcs = [
        ":AsgCapacityHandlerLambdaDeploy",
    ],
    prefix = "jars",
    strip_prefix = ".",
)

java_binary(
    name = "AsgCapacityHandlerLambda",
    runtime_deps = [
        ":AsgCapacityHandler",
    ],
)

java_library(
    name = "TerminatedInstanceHandler",
    srcs = [
        "TerminatedInstanceHandler.java",
        "TerminatedInstanceModule.java",
    ],
    deps = [
        "//java/com/google/scp/operator/autoscaling/tasks/aws:AutoScalingTasks",
        "//java/com/google/scp/operator/shared/dao/asginstancesdb/aws",
        "//java/external:aws_auth",
        "//java/external:aws_auto_scaling",
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
        "//java/external:aws_http_client_spi",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:aws_url_connection_client",
        "//java/external:guice",
    ],
)

alias(
    name = "TerminatedInstanceHandlerLambdaDeploy",
    actual = ":TerminatedInstanceHandlerLambda_deploy.jar",
)

pkg_files(
    name = "TerminatedInstanceHandlerLambdaDeployPackage",
    srcs = [
        ":TerminatedInstanceHandlerLambdaDeploy",
    ],
    prefix = "jars",
    strip_prefix = ".",
)

java_binary(
    name = "TerminatedInstanceHandlerLambda",
    runtime_deps = [
        ":TerminatedInstanceHandler",
    ],
)
