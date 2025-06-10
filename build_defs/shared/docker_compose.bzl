# Copyright 2025 Google LLC
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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

# docker-compose for managing docker containers in PBS integration test

def docker_compose():
    maybe(
        http_file,
        name = "docker_compose",
        downloaded_file_path = "docker-compose",
        sha256 = "9040bd35b2cc0783ce6c5de491de7e52e24d4137dbfc5de8a524f718fc23556c",
        url = "https://github.com/docker/compose/releases/download/v2.36.2/docker-compose-linux-x86_64",
    )
