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

load("@rules_pkg//:mappings.bzl", "pkg_files", "pkg_mklink")

package(default_visibility = ["//visibility:public"])

pkg_files(
    name = "mpkhs_primary_demo_files",
    srcs = [
        "main.tf",
        "mpkhs_primary.auto.tfvars",
    ],
    prefix = "mpkhs_primary",
)

pkg_mklink(
    name = "mpkhs_primary_link",
    link_name = "mpkhs_primary/mpkhs_primary.tf",
    target = "../../shared/mpkhs_primary/mpkhs_primary.tf",
)

pkg_mklink(
    name = "mpkhs_primary_vars_link",
    link_name = "mpkhs_primary/mpkhs_primary_variables.tf",
    target = "../../shared/mpkhs_primary/mpkhs_primary_variables.tf",
)

pkg_mklink(
    name = "mpkhs_primary_outputs_link",
    link_name = "mpkhs_primary/mpkhs_primary_outputs.tf",
    target = "../../shared/mpkhs_primary/mpkhs_primary_outputs.tf",
)
