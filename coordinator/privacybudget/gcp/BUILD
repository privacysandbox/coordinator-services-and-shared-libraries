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

load("@io_bazel_rules_docker//container:container.bzl", "container_push")

container_push(
    name = "pbs_cloud_run_container_image",
    format = "Docker",
    image = "//cc/pbs/deploy/pbs_server/build_defs:reproducible_pbs_cloud_run_container_gcp",
    registry = "",
    repository = "",
    tag = "",
)

container_push(
    name = "pbs_container_image",
    format = "Docker",
    image = "//cc/pbs/deploy/pbs_server/build_defs:reproducible_pbs_container_gcp",
    registry = "",
    repository = "",
    tag = "",
)
