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
    name = "distributedpbs_heartbeat_prober_demo",
    srcs = [
        "demo.auto.tfvars",
        "main.tf",
    ],
    prefix = "distributedpbs_heartbeat_prober",
)

pkg_mklink(
    name = "distributedpbs_heartbeat_prober_link",
    link_name = "distributedpbs_heartbeat_prober/distributedpbs_heartbeat_prober.tf",
    target = "../../shared/distributedpbs_heartbeat_prober/distributedpbs_heartbeat_prober.tf",
)

pkg_mklink(
    name = "distributedpbs_heartbeat_prober_variables_link",
    link_name = "distributedpbs_heartbeat_prober/distributedpbs_heartbeat_prober_variables.tf",
    target = "../../shared/distributedpbs_heartbeat_prober/distributedpbs_heartbeat_prober_variables.tf",
)
