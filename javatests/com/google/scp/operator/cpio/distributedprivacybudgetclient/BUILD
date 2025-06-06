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

load("@rules_java//java:defs.bzl", "java_test")

package(default_visibility = ["//visibility:public"])

java_test(
    name = "DistributedPrivacyBudgetClientTest",
    srcs = ["DistributedPrivacyBudgetClientTest.java"],
    deps = [
        "//java/com/google/scp/coordinator/privacy/budgeting/model",
        "//java/com/google/scp/operator/cpio/distributedprivacybudgetclient:distributed_privacy_budget_client",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:mockito",
        "//java/external:test_parameter_injector",
    ],
)

java_test(
    name = "PrivacyBudgetClientTest",
    srcs = ["PrivacyBudgetClientTest.java"],
    deps = [
        "//java/com/google/scp/coordinator/privacy/budgeting/model",
        "//java/com/google/scp/operator/cpio/distributedprivacybudgetclient:distributed_privacy_budget_client",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/aws/util",
        "//java/com/google/scp/shared/mapper",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:jackson_databind",
        "//java/external:mockito",
    ],
)

java_test(
    name = "PrivacyBudgetClientV2Test",
    srcs = ["PrivacyBudgetClientV2Test.java"],
    deps = [
        "//java/com/google/scp/coordinator/privacy/budgeting/model",
        "//java/com/google/scp/operator/cpio/distributedprivacybudgetclient:distributed_privacy_budget_client",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/aws/util",
        "//java/com/google/scp/shared/mapper",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:jackson_databind",
        "//java/external:mockito",
        "@maven//:com_fasterxml_jackson_core_jackson_core",
    ],
)

java_test(
    name = "TransactionEngineTest",
    srcs = ["TransactionEngineTest.java"],
    deps = [
        "//java/com/google/scp/coordinator/privacy/budgeting/model",
        "//java/com/google/scp/operator/cpio/distributedprivacybudgetclient:distributed_privacy_budget_client",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:mockito",
        # Needs snakeyaml dependency here to choose the right snakeyaml version.
        "//java/external:snakeyaml",
        "//java/external:test_parameter_injector",
    ],
)

java_test(
    name = "TransactionOrchestratorTest",
    srcs = ["TransactionOrchestratorTest.java"],
    deps = [
        "//java/com/google/scp/coordinator/privacy/budgeting/model",
        "//java/com/google/scp/operator/cpio/distributedprivacybudgetclient:distributed_privacy_budget_client",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:mockito",
    ],
)

java_test(
    name = "TransactionPhaseManagerTest",
    srcs = ["TransactionPhaseManagerTest.java"],
    deps = [
        "//java/com/google/scp/operator/cpio/distributedprivacybudgetclient:distributed_privacy_budget_client",
        "//java/external:google_truth",
        "//java/external:guava",
    ],
)

java_test(
    name = "PbsGcpFunctionalTest",
    srcs = ["PbsGcpFunctionalTest.java"],
    deps = [
        "//java/com/google/scp/coordinator/privacy/budgeting/model",
        "//java/com/google/scp/operator/cpio/distributedprivacybudgetclient:distributed_privacy_budget_client",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/aws/credsprovider",
        "//java/com/google/scp/shared/aws/util",
        "//java/com/google/scp/shared/gcp/util",
        "//java/com/google/scp/shared/mapper",
        "//java/external:acai",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:aws_auth",
        "//java/external:aws_regions",
        "//java/external:gcp_spanner",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_databind",
        "//java/external:mockito",
        "//java/external:test_parameter_injector",
        "//java/external:testcontainers",
        "//javatests/com/google/scp/coordinator/privacy/budgeting/utils:pbs_e2e_test_utils",
        "//javatests/com/google/scp/shared/testutils/gcp",
    ],
)
