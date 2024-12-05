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

load(
    "@com_google_googleapis_imports//:imports.bzl",
    "cc_proto_library",
)
load("@rules_pkg//:mappings.bzl", "pkg_filegroup", "pkg_files")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(default_visibility = ["//visibility:private"])

proto_library(
    name = "budget_value_proto",
    srcs = [
        "budget_value.proto",
    ],
)

cc_proto_library(
    name = "budget_value_cc_proto",
    visibility = ["//cc/pbs:__subpackages__"],
    deps = [":budget_value_proto"],
)

pkg_files(
    name = "budget_value_proto_descriptor",
    srcs = [":budget_value_proto"],
    visibility = [
        "//coordinator/terraform/gcp:__subpackages__",
    ],
)
