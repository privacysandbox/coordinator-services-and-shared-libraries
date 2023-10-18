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
    name = "notificationclient",
    srcs = glob([
        "*.java",
    ]),
    deps = [
        ":model",
        "//java/external:guice",
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
    name = "local",
    srcs = glob([
        "*.java",
        "local/*.java",
    ]),
    deps = [
        ":model",
        "//java/external:guice",
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
    ],
)

java_library(
    name = "aws",
    srcs = glob([
        "*.java",
        "aws/*.java",
    ]),
    deps = [
        ":model",
        "//java/com/google/scp/shared/clients/configclient",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:aws_auth",
        "//java/external:aws_core",
        "//java/external:aws_http_client_spi",
        "//java/external:aws_regions",
        "//java/external:aws_sns",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:javax_inject",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
    ],
)

java_library(
    name = "gcp",
    srcs = glob([
        "*.java",
        "gcp/*.java",
    ]),
    deps = [
        ":model",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:gcp_api_common",
        "//java/external:gcp_api_gax",
        "//java/external:gcp_gax_grpc",
        "//java/external:gcp_pubsub",
        "//java/external:gcp_pubsub_v1",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:javax_inject",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//java/external:slf4j",
        "//java/external:slf4j_simple",
    ],
)
