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
    name = "cryptoclient",
    srcs = glob([
        "*.java",
        "local/*.java",
        "model/*.java",
        "testing/*.java",
    ]),
    deps = [
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/api/v1:java_proto",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/crypto/tink",
        "//java/com/google/scp/shared/mapper",
        "//java/com/google/scp/shared/util",
        "//java/external:apache_httpclient",
        "//java/external:apache_httpcore",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//java/external:slf4j",
        "//java/external:tink",
        "//shared/protos/api/v1:java_proto",
    ],
)

java_library(
    name = "aws",
    srcs = glob([
        "*.java",
        "aws/*.java",
        "local/*.java",
        "model/*.java",
        "testing/*.java",
    ]),
    deps = [
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/api/v1:java_proto",
        "//java/com/google/crypto/tink/integration/awskmsv2",
        "//java/com/google/scp/operator/cpio/configclient",
        "//java/com/google/scp/operator/cpio/configclient:aws",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/aws/credsprovider",
        "//java/com/google/scp/shared/clients/configclient:aws",
        "//java/com/google/scp/shared/clients/configclient:model",
        "//java/com/google/scp/shared/crypto/tink",
        "//java/com/google/scp/shared/crypto/tink/aws",
        "//java/com/google/scp/shared/crypto/tink/kmstoolenclave",
        "//java/com/google/scp/shared/mapper",
        "//java/com/google/scp/shared/util",
        "//java/external:apache_httpclient",
        "//java/external:apache_httpcore",
        "//java/external:aws_apache_http",
        "//java/external:aws_auth",
        "//java/external:aws_core",
        "//java/external:aws_http_client_spi",
        "//java/external:aws_regions",
        "//java/external:aws_sts",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//java/external:slf4j",
        "//java/external:tink",
        "//shared/protos/api/v1:java_proto",
    ],
)

java_library(
    name = "gcp",
    srcs = glob([
        "*.java",
        "gcp/*.java",
        "local/*.java",
        "model/*.java",
        "testing/*.java",
    ]),
    deps = [
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/api/v1:java_proto",
        "//java/com/google/scp/operator/cpio/configclient",
        "//java/com/google/scp/operator/cpio/configclient:gcp",
        "//java/com/google/scp/operator/cpio/cryptoclient",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/clients/configclient",
        "//java/com/google/scp/shared/clients/configclient:gcp",
        "//java/com/google/scp/shared/clients/configclient:model",
        "//java/com/google/scp/shared/crypto/tink",
        "//java/com/google/scp/shared/gcp/util",
        "//java/com/google/scp/shared/mapper",
        "//java/com/google/scp/shared/util",
        "//java/external:apache_httpclient",
        "//java/external:apache_httpcore",
        "//java/external:autovalue",
        "//java/external:autovalue_annotations",
        "//java/external:google_api_client",
        "//java/external:google_auth_library_oauth2_http",
        "//java/external:guava",
        "//java/external:guice",
        "//java/external:jackson_annotations",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:javax_inject",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//java/external:slf4j",
        "//java/external:tink",
        "//java/external:tink-gcpkms",
        "//shared/protos/api/v1:java_proto",
    ],
)
