# Copyright 2024 Google LLC
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

load("@com_google_protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@com_google_protobuf//bazel:java_proto_library.bzl", "java_proto_library")
load("@com_google_protobuf//bazel:proto_library.bzl", "proto_library")
load("@io_bazel_rules_go//proto:def.bzl", "go_proto_library")
load("@rules_pkg//:mappings.bzl", "pkg_files")

package(default_visibility = ["//visibility:private"])

proto_library(
    name = "budget_value_proto",
    srcs = [
        "budget_value.proto",
    ],
)

go_proto_library(
    name = "budget_value_go_proto",
    importpath = "cc/pbs/proto/storage/budget_value_go_proto",
    protos = [":budget_value_proto"],
    visibility = ["//go/update_privacy_budget:__subpackages__"],
)

cc_proto_library(
    name = "budget_value_cc_proto",
    visibility = ["//cc/pbs:__subpackages__"],
    deps = [":budget_value_proto"],
)

java_proto_library(
    name = "budget_value_java_proto",
    visibility = ["//javatests:__subpackages__"],
    deps = [":budget_value_proto"],
)

pkg_files(
    name = "budget_value_proto_descriptor",
    srcs = [":budget_value_proto"],
    visibility = [
        "//coordinator/terraform/gcp:__subpackages__",
    ],
)
