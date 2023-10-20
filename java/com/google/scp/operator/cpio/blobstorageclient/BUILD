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
    name = "blobstorageclient",
    srcs = glob([
        "*.java",
        "testing/*.java",
    ]),
    deps = [
        ":model",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:guava",
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
    ],
)

java_library(
    name = "aws",
    srcs = glob([
        "*.java",
        "aws/*.java",
        "testing/*.java",
    ]),
    deps = [
        ":model",
        "//java/com/google/scp/shared/clients/configclient",
        "//java/external:aws_apache_http",
        "//java/external:aws_auth",
        "//java/external:aws_core",
        "//java/external:aws_netty_nio_client",
        "//java/external:aws_regions",
        "//java/external:aws_s3",
        "//java/external:aws_s3_transfer_manager",
        "//java/external:google_gax",
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
        "testing/*.java",
    ]),
    deps = [
        ":model",
        "//java/com/google/scp/operator/cpio/blobstorageclient",
        "//java/com/google/scp/shared/clients/configclient",
        "//java/com/google/scp/shared/clients/configclient:gcp",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:gcp_storage",
        "//java/external:google_auth_library_oauth2_http",
        "//java/external:google_gax",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:javax_inject",
    ],
)
