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
    name = "configclient",
    srcs = glob(["*.java"]),
    deps = [
        "//java/external:guice",
    ],
)

java_library(
    name = "aws",
    srcs = glob([
        "aws/*.java",
        "local/*.java",
        "common/*.java",
    ]),
    deps = [
        "//java/com/google/scp/operator/cpio/configclient",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/aws/credsprovider",
        "//java/com/google/scp/shared/aws/util",
        "//java/com/google/scp/shared/clients",
        "//java/com/google/scp/shared/clients/configclient:aws",
        "//java/com/google/scp/shared/clients/configclient:model",
        "//java/external:apache_httpclient",
        "//java/external:apache_httpclient5",
        "//java/external:apache_httpcore",
        "//java/external:apache_httpcore5",
        "//java/external:aws_apache_http",
        "//java/external:aws_auth",
        "//java/external:aws_core",
        "//java/external:aws_ec2",
        "//java/external:aws_http_client_spi",
        "//java/external:aws_regions",
        "//java/external:aws_ssm",
        "//java/external:aws_sts",
        "//java/external:aws_utils",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:javax_inject",
    ],
)

java_library(
    name = "gcp",
    srcs = glob([
        "gcp/*.java",
        "local/*.java",
        "common/*.java",
    ]),
    deps = [
        "//java/com/google/scp/operator/cpio/configclient",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/clients",
        "//java/com/google/scp/shared/clients/configclient:gcp",
        "//java/com/google/scp/shared/clients/configclient:model",
        "//java/com/google/scp/shared/gcp/util",
        "//java/external:apache_httpclient",
        "//java/external:apache_httpcore",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:gcp_api_gax",
        "//java/external:gcp_secretmanager",
        "//java/external:gcp_secretmanager_proto",
        "//java/external:google_api_client",
        "//java/external:google_auth_library_oauth2_http",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_annotations",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:javax_inject",
        "//java/external:slf4j",
        "//java/external:tink",
        "//java/external:tink-gcpkms",
    ],
)
