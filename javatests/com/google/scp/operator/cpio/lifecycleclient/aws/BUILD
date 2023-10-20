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
    name = "AwsLifecycleClientTest",
    srcs = ["AwsLifecycleClientTest.java"],
    deps = [
        "//java/com/google/scp/operator/cpio/configclient:aws",
        "//java/com/google/scp/operator/cpio/lifecycleclient:aws",
        "//java/com/google/scp/operator/shared/dao/asginstancesdb/aws",
        "//java/com/google/scp/operator/shared/testing",
        "//java/com/google/scp/shared/clients/configclient:aws",
        "//java/com/google/scp/shared/proto",
        "//java/external:aws_auto_scaling",
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guice",
        "//java/external:guice-testlib",
        "//java/external:mockito",
        "//operator/protos/shared/backend/asginstance:java_proto",
    ],
)

java_test(
    name = "AwsLifecycleActionExtenderServiceTest",
    srcs = ["AwsLifecycleActionExtenderServiceTest.java"],
    deps = [
        "//java/com/google/scp/operator/cpio/configclient:aws",
        "//java/com/google/scp/operator/cpio/lifecycleclient:aws",
        "//java/com/google/scp/operator/shared/dao/asginstancesdb/aws",
        "//java/com/google/scp/operator/shared/dao/asginstancesdb/common",
        "//java/com/google/scp/operator/shared/testing",
        "//java/com/google/scp/shared/clients/configclient:aws",
        "//java/com/google/scp/shared/proto",
        "//java/external:aws_auto_scaling",
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guice",
        "//java/external:guice-testlib",
        "//java/external:mockito",
        "//java/external:protobuf_java",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/asginstance:java_proto",
    ],
)
