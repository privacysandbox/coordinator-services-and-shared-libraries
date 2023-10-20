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
    name = "jobclient",
    srcs = glob(["*.java"]),
    deps = [
        ":model",
        "//java/com/google/scp/operator/cpio/lifecycleclient",
        "//java/com/google/scp/operator/cpio/metricclient",
        "//java/com/google/scp/operator/cpio/metricclient:model",
        "//java/com/google/scp/operator/cpio/notificationclient",
        "//java/com/google/scp/operator/cpio/notificationclient:model",
        "//java/com/google/scp/operator/shared/dao/jobqueue/common",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/shared/clients/configclient",
        "//java/com/google/scp/shared/clients/configclient:model",
        "//java/com/google/scp/shared/proto",
        "//java/external:findbugs",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:javax_inject",
        "//java/external:protobuf_java_util",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/jobqueue:java_proto",
        "//operator/protos/shared/backend/metadatadb:java_proto",
    ],
)

java_library(
    name = "model",
    srcs = glob(["model/*.java"]),
    deps = [
        "//java/com/google/scp/operator/shared/model",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:guava",
        "//java/external:jackson_annotations",
        "//operator/protos/shared/backend:java_proto",
    ],
)

java_library(
    name = "aws",
    srcs = glob([
        "*.java",
        "local/*.java",
        "aws/*.java",
        "testing/*.java",
    ]),
    deps = [
        ":model",
        "//java/com/google/scp/operator/cpio/configclient",
        "//java/com/google/scp/operator/cpio/lifecycleclient",
        "//java/com/google/scp/operator/cpio/metricclient",
        "//java/com/google/scp/operator/cpio/metricclient:model",
        "//java/com/google/scp/operator/cpio/notificationclient",
        "//java/com/google/scp/operator/cpio/notificationclient:model",
        "//java/com/google/scp/operator/shared/dao/jobqueue/aws",
        "//java/com/google/scp/operator/shared/dao/jobqueue/common",
        "//java/com/google/scp/operator/shared/dao/metadatadb/aws",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/com/google/scp/operator/shared/dao/metadatadb/testing:job_generator",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/shared/clients/configclient:aws",
        "//java/com/google/scp/shared/clients/configclient:model",
        "//java/com/google/scp/shared/proto",
        "//java/external:aws_apache_http",
        "//java/external:aws_auth",
        "//java/external:aws_core",
        "//java/external:aws_dynamodb",
        "//java/external:aws_dynamodb_enhanced",
        "//java/external:aws_regions",
        "//java/external:aws_sqs",
        "//java/external:findbugs",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:javax_inject",
        "//java/external:protobuf_java_util",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/jobqueue:java_proto",
        "//operator/protos/shared/backend/metadatadb:java_proto",
        "@com_google_protobuf//:protobuf_java",
    ],
)

java_library(
    name = "gcp",
    srcs = glob([
        "*.java",
        "local/*.java",
        "gcp/*.java",
        "testing/*.java",
    ]),
    deps = [
        "//java/com/google/scp/operator/cpio/blobstorageclient:model",
        "//java/com/google/scp/operator/cpio/configclient",
        "//java/com/google/scp/operator/cpio/jobclient",
        "//java/com/google/scp/operator/cpio/jobclient:model",
        "//java/com/google/scp/operator/cpio/lifecycleclient",
        "//java/com/google/scp/operator/cpio/metricclient",
        "//java/com/google/scp/operator/cpio/metricclient:model",
        "//java/com/google/scp/operator/cpio/notificationclient",
        "//java/com/google/scp/operator/cpio/notificationclient:model",
        "//java/com/google/scp/operator/shared/dao/jobqueue/common",
        "//java/com/google/scp/operator/shared/dao/jobqueue/gcp",
        "//java/com/google/scp/operator/shared/dao/metadatadb/common",
        "//java/com/google/scp/operator/shared/dao/metadatadb/gcp",
        "//java/com/google/scp/operator/shared/dao/metadatadb/testing:job_generator",
        "//java/com/google/scp/operator/shared/model",
        "//java/com/google/scp/shared/clients/configclient",
        "//java/com/google/scp/shared/clients/configclient:gcp",
        "//java/com/google/scp/shared/clients/configclient:model",
        "//java/com/google/scp/shared/proto",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:findbugs",
        "//java/external:gcp_api_gax",
        "//java/external:gcp_pubsub",
        "//java/external:gcp_pubsub_v1",
        "//java/external:gcp_spanner",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:javax_inject",
        "//java/external:protobuf_java_util",
        "//operator/protos/shared/backend:java_proto",
        "//operator/protos/shared/backend/jobqueue:java_proto",
        "//operator/protos/shared/backend/metadatadb:java_proto",
    ],
)
