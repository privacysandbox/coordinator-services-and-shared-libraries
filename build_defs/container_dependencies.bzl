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

# Updated as of: 2024-02-28

CONTAINER_DEPS = {
    "amazonlinux_2": {
        "digest": "sha256:6362a029e3bcfe8d0fd560c938f0b82512a45b4ff79851b863dbf0f190088cb8",
        "registry": "index.docker.io",
        "repository": "amazonlinux",
    },
    "java_base": {
        "digest": "sha256:68e2373f7bef9486c08356bd9ffd3b40b56e6b9316c5f6885eb58b1d9093b43d",
        "registry": "gcr.io",
        "repository": "distroless/java17-debian11",
    },
}
