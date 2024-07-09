# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################
# The following map contains container dependencies used by the `container_pull`
# bazel rules. Docker images under the corresponding repository will be pulled
# based on the image digest.
#
# Container dependencies:
#  - amazonlinux_2: The official Amazon Linux image; needed for reproducibly
#    building AL2 binaries (e.g. //cc/aws/proxy)
#  - java_base: Distroless image for running Java.
################################################################################

# Updated as of: 2024-06-28

CONTAINER_DEPS = {
    "amazonlinux_2": {
        "digest": "sha256:1a78072bc7044c910a037dd6678c78fddad7b82faec407df17a0d77f176cb9a4",
        "registry": "index.docker.io",
        "repository": "amazonlinux",
    },
    "java_base": {
        "digest": "sha256:d1ebe3d183e2e6bd09d4fd8f2cf0206693a3bca1858afe393ceb3161b5268f40",
        "registry": "gcr.io",
        "repository": "distroless/java17-debian11",
    },
}
