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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

# Note: Go build rules are an indirect dependency of "io_bazel_rules_docker" and
# a direct dependency of rpmpack. These rules are not used for deploying go code
# at the time of writing.

def go_deps():
    maybe(
        http_archive,
        name = "io_bazel_rules_go",
        sha256 = "f4a9314518ca6acfa16cc4ab43b0b8ce1e4ea64b81c38d8a3772883f153346b8",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.50.1/rules_go-v0.50.1.zip",
            "https://github.com/bazelbuild/rules_go/releases/download/v0.50.1/rules_go-v0.50.1.zip",
        ],
    )

    maybe(
        http_archive,
        name = "bazel_gazelle",
        sha256 = "b760f7fe75173886007f7c2e616a21241208f3d90e8657dc65d36a771e916b6a",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v0.39.1/bazel-gazelle-v0.39.1.tar.gz",
            "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.39.1/bazel-gazelle-v0.39.1.tar.gz",
        ],
    )
