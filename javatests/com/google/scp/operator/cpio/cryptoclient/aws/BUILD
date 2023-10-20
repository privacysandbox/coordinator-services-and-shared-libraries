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
    name = "AwsKmsDecryptionKeyServiceTest",
    srcs = ["AwsKmsDecryptionKeyServiceTest.java"],
    runtime_deps = [
        # LocalStackContainers uses v1 API in the background.
        # However, we don't interface with the v1 API.
        "//java/external:aws_kms_java_sdk1",
    ],
    deps = [
        "//java/com/google/scp/operator/cpio/cryptoclient:aws",
        "//java/external:acai",
        "//java/external:aws_auth",
        "//java/external:aws_kms",
        "//java/external:google_truth",
        "//java/external:guice",
        "//java/external:localstack_containers",
        "//java/external:testcontainers",
        "//java/external:tink",
        "//javatests/com/google/scp/shared/testutils/aws",
        "//javatests/com/google/scp/shared/testutils/crypto:tink_utils",
    ],
)
