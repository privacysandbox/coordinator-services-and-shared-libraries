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
    name = "AwsTinkUtilTest",
    srcs = [
        "AwsTinkUtilTest.java",
    ],
    deps = [
        "//java/com/google/crypto/tink/integration/awskmsv2",
        "//java/com/google/scp/shared/aws/credsprovider",
        "//java/com/google/scp/shared/crypto/tink",
        "//java/com/google/scp/shared/crypto/tink/aws",
        "//java/com/google/scp/shared/crypto/tink/kmstoolenclave",
        "//java/external:aws_arns",
        "//java/external:aws_auth",
        "//java/external:aws_http_client_spi",
        "//java/external:google_truth",
        "//java/external:mockito",
        "//java/external:tink",
    ],
)
