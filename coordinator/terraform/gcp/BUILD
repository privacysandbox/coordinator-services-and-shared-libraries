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

load("@rules_pkg//:mappings.bzl", "pkg_attributes", "pkg_files")
load("@rules_pkg//:pkg.bzl", "pkg_tar")

package(default_visibility = ["//visibility:public"])

pkg_files(
    name = "pbs_dist_files",
    srcs = [
        "//cc/pbs/proto/storage:budget_value_proto_descriptor",
        "//python/privacybudget/gcp/pbs_auth_handler:auth_cloud_function_handler",
    ],
    prefix = "dist",
)

pkg_files(
    name = "pbs_dist_scripts",
    srcs = [
        "//coordinator/privacybudget/gcp/deploy:initialize_pbs",
    ],
    # These scripts are executable
    attributes = pkg_attributes(
        mode = "0755",
    ),
)

# Set of Terraform files for deployment without binaries.
common_files = [
    "//coordinator/terraform/gcp/util_scripts:util_scripts_pkg",
    "//:version_file",
]

mpkhs_files = [
    "//coordinator/terraform/gcp/applications:multipartykeyhosting_applications_pkg",
    "//coordinator/terraform/gcp/environments_mp_primary/demo:mp_primary_coordinator_demo",
    "//coordinator/terraform/gcp/environments_mp_secondary/demo:mp_secondary_coordinator_demo",
    "//coordinator/terraform/gcp/modules:multipartykeyhosting_modules_pkg",
    "//coordinator/terraform/gcp/modules:multipartykeyhosting_modules_shared_pkg",
]

pbs_files = [
    "//coordinator/terraform/gcp/applications:distributedpbs_applications_pkg",
    "//coordinator/terraform/gcp/modules:distributedpbs_modules_pkg",
    ":pbs_dist_files",
    ":pbs_dist_scripts",
]

# Tarball containing files for KHS and PBS deployment.
pkg_tar(
    name = "multiparty_coordinator_tar",
    srcs = common_files + mpkhs_files + pbs_files,
    extension = "tgz",
    strip_prefix = "/",
)

# Tarball containing only files for KHS deployment.
pkg_tar(
    name = "khs_coordinator_tar",
    srcs = common_files + mpkhs_files,
    extension = "tgz",
    strip_prefix = "/",
)
