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

load("@rules_java//java:defs.bzl", "java_library", "java_test")

package(default_visibility = ["//visibility:public"])

java_test(
    name = "PubSubJobQueueTest",
    srcs = ["PubSubJobQueueTest.java"],
    runtime_deps = [
        "//java/external:slf4j_simple",
    ],
    deps = [
        ":gcp",
        "//java/com/google/scp/operator/shared/dao/jobqueue/common",
        "//java/com/google/scp/operator/shared/dao/jobqueue/gcp",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/shared/proto",
        "//java/external:acai",
        "//java/external:gcp_pubsub_v1",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guice",
        "//java/external:protobuf_java_util",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/jobqueue:java_proto",
    ],
)

java_library(
    name = "gcp",
    testonly = True,
    srcs = ["PubSubJobQueueTestModule.java"],
    deps = [
        "//java/com/google/scp/operator/shared/dao/jobqueue/common",
        "//java/com/google/scp/operator/shared/dao/jobqueue/gcp",
        "//java/external:acai",
        "//java/external:gcp_pubsub_v1",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guice",
        "//javatests/com/google/scp/shared/testutils/gcp",
    ],
)
