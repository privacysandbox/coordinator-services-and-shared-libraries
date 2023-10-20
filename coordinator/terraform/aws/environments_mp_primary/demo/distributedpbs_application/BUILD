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
    name = "distributedpbs_application_demo",
    srcs = [
        "main.tf",
        "pbs_app.auto.tfvars",
    ],
    prefix = "distributedpbs_application",
)

pkg_mklink(
    name = "distributedpbs_application_link",
    src = "../../shared/distributedpbs_application/distributedpbs_application.tf",
    dest = "distributedpbs_application/distributedpbs_application.tf",
)

pkg_mklink(
    name = "distributedpbs_application_variables_link",
    src = "../../shared/distributedpbs_application/distributedpbs_application_variables.tf",
    dest = "distributedpbs_application/distributedpbs_application_variables.tf",
)

pkg_mklink(
    name = "distributedpbs_application_outputs_link",
    src = "../../shared/distributedpbs_application/distributedpbs_application_outputs.tf",
    dest = "distributedpbs_application/distributedpbs_application_outputs.tf",
)
