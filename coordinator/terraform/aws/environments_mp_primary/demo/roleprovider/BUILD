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
    name = "role_provider_demo_files",
    srcs = [
        "main.tf",
        "role_provider.auto.tfvars",
    ],
    prefix = "roleprovider",
)

pkg_mklink(
    name = "role_provider_link",
    src = "../../shared/roleprovider/role_provider.tf",
    dest = "roleprovider/role_provider.tf",
)

pkg_mklink(
    name = "role_provider_vars_link",
    src = "../../shared/roleprovider/role_provider_variables.tf",
    dest = "roleprovider/role_provider_variables.tf",
)

pkg_mklink(
    name = "role_provider_outputs_link",
    src = "../../shared/roleprovider/role_provider_outputs.tf",
    dest = "roleprovider/role_provider_outputs.tf",
)
