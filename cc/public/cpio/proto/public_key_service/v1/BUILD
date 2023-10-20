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

load(
    "@com_google_googleapis_imports//:imports.bzl",
    "cc_grpc_library",
    "cc_proto_library",
    "java_grpc_library",
)
load("@rules_java//java:defs.bzl", "java_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "public_key_service_proto",
    srcs = ["public_key_service.proto"],
    deps = [
        "//cc/core/common/proto:core_common_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

##############################################################################
# C++
##############################################################################
cc_proto_library(
    name = "public_key_service_cc_proto",
    deps = [":public_key_service_proto"],
)

cc_grpc_library(
    name = "public_key_service_cc_grpc",
    srcs = [":public_key_service_proto"],
    generate_mocks = True,
    grpc_only = True,
    deps = [":public_key_service_cc_proto"],
)

##############################################################################
# Java
##############################################################################
java_proto_library(
    name = "public_key_service_java_proto",
    deps = [":public_key_service_proto"],
)

java_grpc_library(
    name = "public_key_service_java_grpc",
    srcs = [":public_key_service_proto"],
    deps = [":public_key_service_java_proto"],
)
