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
    name = "multipartykeyhosting_applications_pkg",
    srcs = [
        "//coordinator/terraform/gcp/applications/allowedoperatorgroup",
        "//coordinator/terraform/gcp/applications/multipartykeyhosting_primary",
        "//coordinator/terraform/gcp/applications/multipartykeyhosting_secondary",
        "//coordinator/terraform/gcp/applications/operator_workloadidentitypoolprovider",
        "//coordinator/terraform/gcp/applications/setupresources",
    ],
    prefix = "applications",
)

pkg_filegroup(
    name = "distributedpbs_applications_pkg",
    srcs = [
        "//coordinator/terraform/gcp/applications/distributedpbs_application",
        "//coordinator/terraform/gcp/applications/distributedpbs_base",
    ],
    prefix = "applications",
)
