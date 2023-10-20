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

load("@rules_java//java:defs.bzl", "java_binary", "java_library")
load("@rules_pkg//:pkg.bzl", "pkg_zip")

package(default_visibility = ["//visibility:public"])

java_library(
    name = "GcpKeyServiceModule",
    srcs = [
        "GcpKeyServiceModule.java",
    ],
    deps = [
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/common",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/tasks",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/dao/gcp",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/external:guice",
    ],
)

java_library(
    name = "PublicKeyServiceHttpFunction",
    srcs = [
        "GetPublicKeysRequestHandler.java",
        "PublicKeyServiceHttpFunction.java",
        "PublicKeyServiceHttpFunctionBase.java",
    ],
    deps = [
        ":GcpKeyServiceModule",
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/common",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/gcp/util",
        "//java/external:gcp_cloud_function",
        "//java/external:guava",
        "//java/external:guice",
    ],
)

java_binary(
    name = "PublicKeyServiceHttpCloudFunction",
    runtime_deps = [
        ":PublicKeyServiceHttpFunction",
    ],
)

alias(
    name = "PublicKeyServiceHttpCloudFunctionDeploy",
    actual = ":PublicKeyServiceHttpCloudFunction_deploy.jar",
)

pkg_zip(
    name = "PublicKeyServiceHttpCloudFunctionDeploy_zip",
    srcs = [
        ":PublicKeyServiceHttpCloudFunction_deploy.jar",
    ],
)

java_library(
    name = "EncryptionKeyServiceHttpFunction",
    srcs = [
        "EncryptionKeyServiceHttpFunction.java",
        "EncryptionKeyServiceHttpFunctionBase.java",
        "GetEncryptionKeyRequestHandler.java",
        "ListRecentEncryptionKeysRequestHandler.java",
    ],
    deps = [
        ":GcpKeyServiceModule",
        "//coordinator/protos/keymanagement/keyhosting/api/v1:java_proto",
        "//coordinator/protos/keymanagement/shared/api/v1:java_proto",
        "//java/com/google/scp/coordinator/keymanagement/keyhosting/service/common",
        "//java/com/google/scp/coordinator/keymanagement/shared/model",
        "//java/com/google/scp/shared/api/exception",
        "//java/com/google/scp/shared/api/model",
        "//java/com/google/scp/shared/api/util",
        "//java/com/google/scp/shared/gcp/util",
        "//java/external:gcp_cloud_function",
        "//java/external:guava",
        "//java/external:guice",
    ],
)

java_binary(
    name = "EncryptionKeyServiceHttpCloudFunction",
    runtime_deps = [
        ":EncryptionKeyServiceHttpFunction",
    ],
)

alias(
    name = "EncryptionKeyServiceHttpCloudFunctionDeploy",
    actual = ":EncryptionKeyServiceHttpCloudFunction_deploy.jar",
)

pkg_zip(
    name = "EncryptionKeyServiceHttpCloudFunctionDeploy_zip",
    srcs = [
        ":EncryptionKeyServiceHttpCloudFunction_deploy.jar",
    ],
)
