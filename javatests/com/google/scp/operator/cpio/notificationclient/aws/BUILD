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

load("@rules_java//java:defs.bzl", "java_test")

package(default_visibility = ["//visibility:public"])

java_test(
    name = "AwsNotificationClientTest",
    srcs = ["AwsNotificationClientTest.java"],
    deps = [
        "//java/com/google/scp/operator/cpio/notificationclient",
        "//java/com/google/scp/operator/cpio/notificationclient:aws",
        "//java/com/google/scp/operator/cpio/notificationclient:model",
        "//java/external:aws_sdk",
        "//java/external:aws_sns",
        "//java/external:google_truth",
        "//java/external:grpc_api",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:mockito",
        "//javatests/com/google/scp/shared/testutils/aws",
    ],
)
