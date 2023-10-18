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

package(default_visibility = [
    "//java/com/google/scp/coordinator/pbsprober:__subpackages__",
    "//javatests/com/google/scp/coordinator/pbsprober:__subpackages__",
])

java_library(
    name = "Annotations",
    srcs = [
        "Annotations.java",
    ],
    deps = [
        "//java/external:guice",
        "//java/external:javax_inject",
    ],
)

java_library(
    name = "SystemEnvironmentVariableModule",
    srcs = [
        "SystemEnvironmentVariableModule.java",
    ],
    deps = [
        ":Annotations",
        "//java/com/google/scp/operator/cpio/distributedprivacybudgetclient:distributed_privacy_budget_client",
        "//java/external:guava",
        "//java/external:guice",
    ],
)

java_library(
    name = "RandomPrivacyBudgetConsumer",
    srcs = [
        "RandomPrivacyBudgetConsumer.java",
    ],
    deps = [
        ":Annotations",
        "//java/com/google/scp/coordinator/pbsprober/model",
        "//java/com/google/scp/coordinator/privacy/budgeting/model",
        "//java/com/google/scp/operator/cpio/distributedprivacybudgetclient:distributed_privacy_budget_client",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:javax_inject",
    ],
)
