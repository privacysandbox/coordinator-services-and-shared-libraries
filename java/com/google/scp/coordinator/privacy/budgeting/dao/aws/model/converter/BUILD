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

load("@rules_java//java:defs.bzl", "java_library")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "instant_milliseconds_converter",
    srcs = ["InstantMillisecondsConverter.java"],
    deps = [
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
    ],
)

java_library(
    name = "instant_seconds_converter",
    srcs = ["InstantSecondsConverter.java"],
    deps = [
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
    ],
)

java_library(
    name = "aws_privacy_budget_converter",
    srcs = ["AwsPrivacyBudgetConverter.java"],
    deps = [
        "//java/com/google/scp/coordinator/privacy/budgeting/dao/aws/model:aws_privacy_budget",
        "//java/com/google/scp/coordinator/privacy/budgeting/dao/model:privacy_budget",
        "//java/external:guava",
    ],
)
