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

load("@rules_java//java:defs.bzl", "java_test")

package(default_visibility = ["//visibility:public"])

java_test(
    name = "ErrorUtilTest",
    srcs = [
        "ErrorUtilTest.java",
    ],
    deps = [
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/mapper",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//shared/protos/api/v1:java_proto",
    ],
)

java_test(
    name = "RequestUtilTest",
    srcs = [
        "RequestUtilTest.java",
    ],
    deps = [
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/external:google_truth",
        "//java/external:guava",
    ],
)

java_test(
    name = "HttpClientWithInterceptorTest",
    srcs = ["HttpClientWithInterceptorTest.java"],
    tags = [
        "exclusive",
    ],
    deps = [
        "//java/com/google/scp/shared/api/util",
        "//java/external:apache_httpclient5",
        "//java/external:apache_httpcore5",
        "//java/external:aws_auth",
        "//java/external:aws_sts",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:mockito",
        "//java/external:mockito_inline",
        "//java/external:resilience4j_core",
        "//java/external:resilience4j_retry",
    ],
)

java_test(
    name = "HttpClientWrapperTest",
    srcs = ["HttpClientWrapperTest.java"],
    deps = [
        "//java/com/google/scp/shared/api/util",
        "//java/external:apache_httpclient",
        "//java/external:apache_httpcore",
        "//java/external:aws_sts",
        "//java/external:google_truth",
        "//java/external:guava",
        "//java/external:mockito",
        "//java/external:resilience4j_retry",
    ],
)
