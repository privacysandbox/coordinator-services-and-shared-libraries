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
load("@rules_pkg//:mappings.bzl", "pkg_files")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "AwsSystemEnvironmentVariableModule",
    srcs = [
        "AwsSystemEnvironmentVariableModule.java",
    ],
    deps = [
        "//java/com/google/scp/coordinator/pbsprober:Annotations",
        "//java/com/google/scp/operator/cpio/configclient",
        "//java/com/google/scp/operator/cpio/distributedprivacybudgetclient:distributed_privacy_budget_client",
        "//java/external:aws_regions",
        "//java/external:guava",
        "//java/external:guice",
    ],
)

java_library(
    name = "AwsPrivacyBudgetProberModule",
    srcs = [
        "AwsPrivacyBudgetProberModule.java",
    ],
    deps = [
        "//java/com/google/scp/coordinator/pbsprober:Annotations",
        "//java/com/google/scp/operator/cpio/configclient",
        "//java/com/google/scp/operator/cpio/configclient:aws",
        "//java/com/google/scp/operator/cpio/distributedprivacybudgetclient:distributed_privacy_budget_client",
        "//java/com/google/scp/operator/cpio/distributedprivacybudgetclient/aws:aws_pbs_client_module",
        "//java/com/google/scp/shared/aws/credsprovider",
        "//java/com/google/scp/shared/clients/configclient",
        "//java/com/google/scp/shared/clients/configclient:aws",
        "//java/external:aws_auth",
        "//java/external:aws_regions",
        "//java/external:aws_sts",
        "//java/external:guice",
        "//java/external:javax_inject",
    ],
)

java_library(
    name = "PrivacyBudgetProberHandler",
    srcs = [
        "PrivacyBudgetProberHandler.java",
    ],
    deps = [
        ":AwsPrivacyBudgetProberModule",
        ":AwsSystemEnvironmentVariableModule",
        "//java/com/google/scp/coordinator/pbsprober:RandomPrivacyBudgetConsumer",
        "//java/com/google/scp/coordinator/pbsprober:SystemEnvironmentVariableModule",
        "//java/com/google/scp/coordinator/pbsprober/model",
        "//java/com/google/scp/coordinator/privacy/budgeting/model",
        "//java/com/google/scp/operator/cpio/distributedprivacybudgetclient:distributed_privacy_budget_client",
        "//java/external:aws_lambda_java_core",
        "//java/external:aws_lambda_java_events",
        "//java/external:guava",
        "//java/external:guice",
    ],
)

java_binary(
    name = "PrivacyBudgetProberHandlerLambda",
    runtime_deps = [
        ":PrivacyBudgetProberHandler",
    ],
)

alias(
    name = "PrivacyBudgetProberHandlerLambdaDeploy",
    actual = ":PrivacyBudgetProberHandlerLambda_deploy.jar",
)

pkg_files(
    name = "PrivacyBudgetProberHandlerLambdaDeployPackage",
    srcs = [
        ":PrivacyBudgetProberHandlerLambdaDeploy",
    ],
    prefix = "jars",
    strip_prefix = ".",
)
