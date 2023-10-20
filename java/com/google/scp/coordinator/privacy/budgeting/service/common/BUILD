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
    name = "annotations",
    srcs = ["Annotations.java"],
    deps = [
        "//java/external:guice",
    ],
)

java_library(
    name = "privacy_budgeting_service_module",
    srcs = ["PrivacyBudgetingServiceModule.java"],
    deps = [
        "//java/com/google/scp/coordinator/privacy/budgeting/dao/common:privacy_budget_database_bridge",
        "//java/com/google/scp/coordinator/privacy/budgeting/service/common:annotations",
        "//java/external:guice",
    ],
)
