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

load("@rules_java//java:defs.bzl", "java_test")

package(default_visibility = ["//visibility:public"])

java_test(
    name = "GcpMetadataServiceClientTest",
    srcs = [
        "GcpMetadataServiceClientTest.java",
    ],
    deps = [
        "//java/com/google/scp/shared/clients/configclient:gcp",
        "//java/external:apache_httpclient",
        "//java/external:apache_httpcore",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guice",
        "//java/external:mockito",
    ],
)

java_test(
    name = "GcpParameterClientTest",
    srcs = [
        "GcpParameterClientTest.java",
    ],
    deps = [
        "//java/com/google/scp/shared/clients/configclient:aws",
        "//java/com/google/scp/shared/clients/configclient:gcp",
        "//java/external:gcp_api_gax",
        "//java/external:gcp_gax_grpc",
        "//java/external:gcp_secretmanager",
        "//java/external:gcp_secretmanager_proto",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:grpc_api",
        "//java/external:guice",
        "//java/external:mockito",
        "//java/external:protobuf_java",
    ],
)

java_test(
    name = "CredentialsHelperTest",
    srcs = [
        "CredentialsHelperTest.java",
    ],
    deps = [
        "//java/com/google/scp/shared/clients/configclient:gcp",
        "//java/external:google_api_client",
        "//java/external:google_auth_library_oauth2_http",
        "//java/external:google_truth",
        "//java/external:google_truth8",
    ],
)

java_test(
    name = "GcpParameterClientManualTest",
    srcs = [
        "GcpParameterClientManualTest.java",
    ],
    tags = ["manual"],
    deps = [
        "//java/com/google/scp/shared/clients/configclient:aws",
        "//java/com/google/scp/shared/clients/configclient:gcp",
        "//java/external:gcp_secretmanager",
        "//java/external:gcp_secretmanager_proto",
        "//java/external:google_truth",
        "//java/external:google_truth8",
        "//java/external:guice",
        "//java/external:mockito",
        "//java/external:protobuf_java",
    ],
)
