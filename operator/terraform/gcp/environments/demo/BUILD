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

# pkg_tar requires links be declared with mklink, otherwise the linked file is
# copied.
pkg_mklink(
    name = "job_service_link",
    src = "../shared/job_service.tf",
    dest = "demo/job_service.tf",
)

pkg_mklink(
    name = "job_service_vars_link",
    src = "../shared/job_service_variables.tf",
    dest = "demo/job_service_variables.tf",
)

pkg_mklink(
    name = "job_service_outputs_link",
    src = "../shared/job_service_outputs.tf",
    dest = "demo/job_service_outputs.tf",
)

pkg_files(
    name = "demo_files",
    srcs = [
        "example.auto.tfvars",
        "main.tf",
    ],
    prefix = "demo",
)