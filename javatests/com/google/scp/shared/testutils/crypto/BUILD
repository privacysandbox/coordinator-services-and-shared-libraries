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
    name = "tink_utils",
    srcs = [
        "AwsTinkUtilsModule.java",
        "MockTinkUtils.java",
        "TinkUtils.java",
    ],
    deps = [
        "//java/com/google/crypto/tink/integration/awskmsv2",
        "//java/com/google/scp/operator/cpio/cryptoclient:aws",
        "//java/com/google/scp/shared/util",
        "//java/external:acai",
        "//java/external:aws_apache_http",
        "//java/external:aws_auth",
        "//java/external:guice",
        "//java/external:localstack_containers",
        "//java/external:mockito",
        "//java/external:protobuf_java",
        "//java/external:tink",
        "//javatests/com/google/scp/shared/testutils/aws",
    ],
)
