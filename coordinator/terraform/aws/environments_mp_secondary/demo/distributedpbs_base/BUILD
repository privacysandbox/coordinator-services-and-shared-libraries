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

load("@rules_pkg//:mappings.bzl", "pkg_files", "pkg_mklink")

package(default_visibility = ["//visibility:public"])

pkg_files(
    name = "distributedpbs_base_demo",
    srcs = [
        "main.tf",
        "pbs_base.auto.tfvars",
    ],
    prefix = "distributedpbs_base",
)

pkg_mklink(
    name = "distributedpbs_base_link",
    src = "../../shared/distributedpbs_base/distributedpbs_base.tf",
    dest = "distributedpbs_base/distributedpbs_base.tf",
)

pkg_mklink(
    name = "distributedpbs_base_variables_link",
    src = "../../shared/distributedpbs_base/distributedpbs_base_variables.tf",
    dest = "distributedpbs_base/distributedpbs_base_variables.tf",
)

pkg_mklink(
    name = "distributedpbs_base_outputs_link",
    src = "../../shared/distributedpbs_base/distributedpbs_base_outputs.tf",
    dest = "distributedpbs_base/distributedpbs_base_outputs.tf",
)
