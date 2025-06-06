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

load("@rules_java//java:defs.bzl", "java_library")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "gcp",
    srcs = glob(["*.java"]),
    deps = [
        "//java/com/google/scp/operator/shared/dao/asginstancesdb/common",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/shared/gcp/util",
        "//java/com/google/scp/shared/proto",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:gcp_core",
        "//java/external:gcp_spanner",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/asginstance:java_proto",
        "//operator/protos/shared/backend/metadatadb:java_proto",
    ],
)
