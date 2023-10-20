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
    name = "SpannerMetadataDbTest",
    srcs = ["SpannerMetadataDbTest.java"],
    runtime_deps = [
        "//java/external:slf4j_simple",
    ],
    deps = [
        ":gcp",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/com/google/scp/operator/shared/dao/metadatadb/gcp",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/operator/shared/testing",
        "//java/com/google/scp/shared/proto",
        "//java/external:acai",
        "//java/external:gcp_core",
        "//java/external:gcp_spanner",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guice",
        "//java/external:jackson_databind",
        "//java/external:protobuf_java_util",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/metadatadb:java_proto",
    ],
)

java_test(
    name = "SpannerAsgInstancesDaoTest",
    srcs = ["SpannerAsgInstancesDaoTest.java"],
    runtime_deps = [
        "//java/external:slf4j_simple",
    ],
    deps = [
        ":gcp",
        "//java/com/google/scp/operator/shared/dao/asginstancesdb/common",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/com/google/scp/operator/shared/dao/metadatadb/gcp",
        "//java/com/google/scp/shared/proto",
        "//java/external:acai",
        "//java/external:gcp_core",
        "//java/external:gcp_spanner",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guice",
        "//java/external:jackson_databind",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/asginstance:java_proto",
    ],
)

java_library(
    name = "gcp",
    testonly = True,
    srcs = ["SpannerMetadataDbTestModule.java"],
    deps = [
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/com/google/scp/operator/shared/dao/metadatadb/gcp",
        "//java/com/google/scp/shared/mapper",
        "//java/external:acai",
        "//java/external:gcp_core",
        "//java/external:gcp_spanner",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guice",
        "//java/external:jackson_databind",
        "//javatests/com/google/scp/shared/gcp:constants",
        "//javatests/com/google/scp/shared/testutils/gcp",
    ],
)
