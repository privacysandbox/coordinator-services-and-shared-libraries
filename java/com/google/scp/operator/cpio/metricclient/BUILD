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
    name = "metricclient",
    srcs = glob([
        "*.java",
        "local/*.java",
    ]),
    deps = [
        ":model",
        "//java/external:guice",
        "//java/external:javax_inject",
    ],
)

java_library(
    name = "model",
    srcs = glob(["model/*.java"]),
    deps = [
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:guava",
        "//java/external:jackson_annotations",
    ],
)

java_library(
    name = "aws",
    srcs = glob([
        "*.java",
        "aws/*.java",
        "local/*.java",
    ]),
    deps = [
        ":model",
        "//java/com/google/scp/operator/cpio/configclient",
        "//java/com/google/scp/operator/cpio/metricclient",
        "//java/com/google/scp/shared/clients/configclient",
        "//java/external:aws_auth",
        "//java/external:aws_cloudwatch",
        "//java/external:aws_core",
        "//java/external:aws_http_client_spi",
        "//java/external:aws_regions",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:javax_inject",
    ],
)

java_library(
    name = "gcp",
    srcs = glob([
        "*.java",
        "gcp/*.java",
        "local/*.java",
    ]),
    deps = [
        ":model",
        "//java/com/google/scp/operator/cpio/configclient",
        "//java/com/google/scp/operator/cpio/metricclient",
        "//java/com/google/scp/shared/clients/configclient",
        "//java/com/google/scp/shared/clients/configclient:gcp",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:gcp_api_gax",
        "//java/external:gcp_cloud_monitoring",
        "//java/external:gcp_monitoring_v3",
        "//java/external:google_common_proto",
        "//java/external:grpc_api",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:javax_inject",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
    ],
)
