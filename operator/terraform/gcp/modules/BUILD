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

load("@rules_pkg//:mappings.bzl", "pkg_filegroup")

package(default_visibility = ["//visibility:public"])

pkg_filegroup(
    name = "modules_pkg",
    srcs = [
        "//operator/terraform/gcp/modules/adtech_setup",
        "//operator/terraform/gcp/modules/autoscaling",
        "//operator/terraform/gcp/modules/bazel",
        "//operator/terraform/gcp/modules/frontend",
        "//operator/terraform/gcp/modules/jobqueue",
        "//operator/terraform/gcp/modules/metadatadb",
        "//operator/terraform/gcp/modules/notifications",
        "//operator/terraform/gcp/modules/parameters",
        "//operator/terraform/gcp/modules/vpc",
        "//operator/terraform/gcp/modules/worker",
    ],
    prefix = "modules",
)
