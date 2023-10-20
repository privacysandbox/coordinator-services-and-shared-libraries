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
        "//operator/terraform/aws/modules/alarmsnstopic",
        "//operator/terraform/aws/modules/asginstancesdb",
        "//operator/terraform/aws/modules/autoscaling",
        "//operator/terraform/aws/modules/bazel",
        "//operator/terraform/aws/modules/frontend",
        "//operator/terraform/aws/modules/frontenddashboard",
        "//operator/terraform/aws/modules/jobqueue",
        "//operator/terraform/aws/modules/metadatadb",
        "//operator/terraform/aws/modules/notifications",
        "//operator/terraform/aws/modules/parameters",
        "//operator/terraform/aws/modules/vpc",
        "//operator/terraform/aws/modules/worker",
        "//operator/terraform/aws/modules/workerdashboard",
    ],
    prefix = "modules",
)
