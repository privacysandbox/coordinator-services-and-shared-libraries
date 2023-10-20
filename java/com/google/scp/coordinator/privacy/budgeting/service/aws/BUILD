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
    name = "aws_consume_privacy_budget_handler",
    srcs = [
        "AwsConsumePrivacyBudgetHandler.java",
        "AwsIamRoles.java",
        "AwsPrivacyBudgetAccountService.java",
    ],
    deps = [
        "//java/com/google/scp/coordinator/privacy/budgeting/dao/aws",
        "//java/com/google/scp/coordinator/privacy/budgeting/dao/common:privacy_budget_database_bridge",
        "//java/com/google/scp/coordinator/privacy/budgeting/dao/model:privacy_budget",
        "//java/com/google/scp/coordinator/privacy/budgeting/dao/model/converter:privacy_budget_unit_converter",
        "//java/com/google/scp/coordinator/privacy/budgeting/model",
        "//java/com/google/scp/coordinator/privacy/budgeting/service/common:annotations",
        "//java/com/google/scp/coordinator/privacy/budgeting/service/common:privacy_budgeting_service_module",
        "//java/com/google/scp/coordinator/privacy/budgeting/service/common/validation:privacy_budgeting_service_validations",
        "//java/com/google/scp/coordinator/privacy/budgeting/tasks:consume_privacy_budget_task",
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
        "//java/external:slf4j",
    ],
)

java_library(
    name = "aws_privacy_budgeting_service_module",
    srcs = ["AwsPrivacyBudgetingServiceModule.java"],
    deps = [
        "//java/com/google/scp/coordinator/privacy/budgeting/dao/aws",
        "//java/com/google/scp/coordinator/privacy/budgeting/dao/aws/model/converter:aws_privacy_budget_converter",
        "//java/com/google/scp/coordinator/privacy/budgeting/dao/common:privacy_budget_database_bridge",
        "//java/com/google/scp/coordinator/privacy/budgeting/dao/model/converter:privacy_budget_unit_converter",
        "//java/com/google/scp/coordinator/privacy/budgeting/service/common:annotations",
        "//java/com/google/scp/coordinator/privacy/budgeting/service/common:privacy_budgeting_service_module",
        "//java/com/google/scp/shared/mapper",
        "//java/external:autoservice",
        "//java/external:autoservice_annotations",
        "//java/external:aws_apache_http",
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
        "//java/external:commons_logging",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_databind",
    ],
)

java_binary(
    name = "aws_consume_privacy_budget_handler_binary",
    runtime_deps = [
        ":aws_consume_privacy_budget_handler",
        ":aws_privacy_budgeting_service_module",
    ],
)

alias(
    name = "aws_consume_privacy_budget_handler_binary_deploy",
    actual = ":aws_consume_privacy_budget_handler_binary_deploy.jar",
)

pkg_files(
    name = "aws_consume_privacy_budget_handler_binary_DeployPackage",
    srcs = [":aws_consume_privacy_budget_handler_binary_deploy"],
    prefix = "jars",
    strip_prefix = ".",
)
