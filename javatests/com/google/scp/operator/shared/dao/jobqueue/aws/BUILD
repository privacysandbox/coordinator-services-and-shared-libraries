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
    name = "SqsJobQueueTest",
    srcs = ["SqsJobQueueTest.java"],
    runtime_deps = [
        # LocalStackContainers uses v1 API in the background.
        # However, we don't interface with the v1 API.
        # TODO: Find a way to not have to copy this to all other AWS tests.
        "//java/external:aws_sqs_java_sdk1",
    ],
    deps = [
        "//java/com/google/scp/operator/shared/dao/jobqueue/aws",
        "//java/com/google/scp/operator/shared/dao/jobqueue/common",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/shared/proto",
        "//java/external:acai",
        "//java/external:aws_auth",
        "//java/external:aws_regions",
        "//java/external:aws_sqs",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:localstack_containers",
        "//java/external:protobuf_java_util",
        "//java/external:testcontainers",
        "//javatests/com/google/scp/shared/testutils/aws",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/jobqueue:java_proto",
    ],
)
