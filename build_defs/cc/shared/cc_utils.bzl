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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def cc_utils():
    maybe(
        new_git_repository,
        name = "nlohmann_json",
        build_file = Label("//build_defs/cc/shared/build_targets:nlohmann.BUILD"),
        # Commits on Apr 6, 2022
        commit = "15fa6a342af7b51cb51a22599026e01f1d81957b",
        remote = "https://github.com/nlohmann/json.git",
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
        name = "curl",
        build_file = Label("//build_defs/cc/shared/build_targets:curl.BUILD"),
        sha256 = "ff3e80c1ca6a068428726cd7dd19037a47cc538ce58ef61c59587191039b2ca6",
        strip_prefix = "curl-7.49.1",
        urls = [
            "https://mirror.bazel.build/curl.haxx.se/download/curl-7.49.1.tar.gz",
        ],
    )

    maybe(
        new_git_repository,
        name = "moodycamel_concurrent_queue",
        build_file = Label("//build_defs/cc/shared/build_targets:moodycamel.BUILD"),
        # Commited Mar 20, 2022
        commit = "22c78daf65d2c8cce9399a29171676054aa98807",
        remote = "https://github.com/cameron314/concurrentqueue.git",
        shallow_since = "1647803790 -0400",
    )
