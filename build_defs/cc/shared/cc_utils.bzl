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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def cc_utils():
    maybe(
        http_archive,
        name = "com_github_nlohmann_json",
        sha256 = "0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406",
        strip_prefix = "json-3.11.3",
        patches = [Label("//build_defs/cc/shared/build_targets:nlohmann.patch")],
        urls = [
            "https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz",
        ],
    )

    maybe(
        git_repository,
        name = "oneTBB",
        # Commits on Apr 6, 2023, v2021.9.0
        commit = "a00cc3b8b5fb4d8115e9de56bf713157073ed68c",
        remote = "https://github.com/oneapi-src/oneTBB.git",
    )

    maybe(
        http_archive,
        name = "com_github_curl_curl",
        patches = [Label("//build_defs/cc/shared/build_targets:curl.patch")],
        sha256 = "77c0e1cd35ab5b45b659645a93b46d660224d0024f1185e8a95cdb27ae3d787d",
        strip_prefix = "curl-8.8.0",
        urls = [
            "https://github.com/curl/curl/releases/download/curl-8_8_0/curl-8.8.0.tar.gz",
        ],
    )
