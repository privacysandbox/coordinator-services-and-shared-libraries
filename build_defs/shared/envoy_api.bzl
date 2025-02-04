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
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def envoy_api():
    maybe(
        http_archive,
        name = "com_envoyproxy_protoc_gen_validate",
        sha256 = "e4718352754df1393b8792b631338aa8562f390e8160783e365454bc11d96328",
        strip_prefix = "protoc-gen-validate-1.2.1",
        url = "https://github.com/bufbuild/protoc-gen-validate/archive/refs/tags/v1.2.1.tar.gz",
    )
    maybe(
        git_repository,
        name = "envoy_api",
        # Commit from January 24, 2025.
        commit = "7714d2d92d64374060871ac4aa70a1d1faae5c8a",
        remote = "https://github.com/envoyproxy/data-plane-api.git",
    )
