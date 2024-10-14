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

# Updated as of: 2024-10-11

CONTAINER_DEPS = {
    "amazonlinux_2": {
        "digest": "sha256:5e71ba9ef75292166e7aad93a6e3bd37518e77fb3c627362b3083a25b49a8f79",
        "registry": "index.docker.io",
        "repository": "amazonlinux",
    },
    "java_base": {
        "digest": "sha256:587ce66b08faea2e2e1568d6bb6c5fd6b085909621f4c14762206d687ff7d202",
        "registry": "gcr.io",
        "repository": "distroless/java17-debian11",
    },
}
