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
    name = "LambdaHandlerUtilTest",
    srcs = ["LambdaHandlerUtilTest.java"],
    deps = [
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//java/com/google/scp/coordinator/privacy/budgeting/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/aws/util",
        "//java/com/google/scp/shared/mapper",
        "//java/external:aws_lambda_java_events",
        "//java/external:google_truth",
        "//java/external:jackson_core",
        "//java/external:jackson_databind",
        "//java/external:mockito",
        "//java/external:protobuf_java",
        "//java/external:protobuf_java_util",
        "//shared/protos/api/v1:java_proto",
    ],
)

java_test(
    name = "AwsRequestSignerTest",
    srcs = ["AwsRequestSignerTest.java"],
    deps = [
        "//java/com/google/scp/shared/aws/util",
        "//java/external:apache_httpclient",
        "//java/external:apache_httpclient5",
        "//java/external:apache_httpcore",
        "//java/external:apache_httpcore5",
        "//java/external:aws_auth",
        "//java/external:aws_regions",
        "//java/external:google_truth",
    ],
)

java_test(
    name = "AwsAuthTokenInterceptorTest",
    srcs = glob(["AwsAuthTokenInterceptorTest.java"]),
    tags = [
        "exclusive",
    ],
    deps = [
        "//java/com/google/scp/operator/shared/testing",
        "//java/com/google/scp/shared/aws/credsprovider",
        "//java/com/google/scp/shared/aws/util",
        "//java/external:apache_httpclient5",
        "//java/external:apache_httpcore5",
        "//java/external:aws_auth",
        "//java/external:aws_regions",
        "//java/external:google_truth",
        "//java/external:junit",
        "//java/external:mockito",
        "//java/external:mockito_inline",
    ],
)
