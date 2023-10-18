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
    name = "aws_pbs_client_module",
    srcs = glob([
        "*.java",
    ]),
    deps = [
        "//java/com/google/scp/operator/cpio/configclient",
        "//java/com/google/scp/operator/cpio/configclient:aws",
        "//java/com/google/scp/operator/cpio/distributedprivacybudgetclient:distributed_privacy_budget_client",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/aws/credsprovider",
        "//java/com/google/scp/shared/aws/util",
        "//java/com/google/scp/shared/clients/configclient:aws",
        "//java/com/google/scp/shared/mapper",
        "//java/external:apache_httpcore5",
        "//java/external:aws_auth",
        "//java/external:aws_regions",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_annotations",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:slf4j",
    ],
)
