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

load("@rules_pkg//:mappings.bzl", "pkg_files")

package(default_visibility = ["//visibility:public"])

pkg_files(
    name = "multipartykeyhosting_primary_module_pkg",
    srcs = [
        "//coordinator/terraform/aws/applications/multipartykeyhosting_primary",
    ],
    prefix = "applications/multipartykeyhosting_primary",
    strip_prefix = ".",
)

pkg_files(
    name = "multipartykeyhosting_secondary_module_pkg",
    srcs = [
        "//coordinator/terraform/aws/applications/multipartykeyhosting_secondary",
    ],
    prefix = "applications/multipartykeyhosting_secondary",
    strip_prefix = ".",
)

pkg_files(
    name = "domainrecords_module_pkg",
    srcs = [
        "//coordinator/terraform/aws/applications/domainrecords",
    ],
    prefix = "applications/domainrecords",
    strip_prefix = ".",
)

pkg_files(
    name = "roleprovider_module_pkg",
    srcs = [
        "//coordinator/terraform/aws/applications/roleprovider",
    ],
    prefix = "applications/roleprovider",
    strip_prefix = ".",
)

pkg_files(
    name = "privacybudgetmanagement_module_pkg",
    srcs = [
        "//coordinator/terraform/aws/applications/privacybudgetmanagement",
    ],
    prefix = "applications/privacybudgetmanagement",
    strip_prefix = ".",
)

pkg_files(
    name = "distributed_pbs_base_pkg",
    srcs = [
        "//coordinator/terraform/aws/applications/distributedpbs_base",
    ],
    prefix = "applications/distributedpbs_base",
    strip_prefix = ".",
)

pkg_files(
    name = "distributed_pbs_application_pkg",
    srcs = [
        "//coordinator/terraform/aws/applications/distributedpbs_application",
    ],
    prefix = "applications/distributedpbs_application",
    strip_prefix = ".",
)
