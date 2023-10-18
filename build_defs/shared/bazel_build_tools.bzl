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

# Note: requires golang toolchain.

def bazel_build_tools():
    maybe(
        http_archive,
        name = "com_github_bazelbuild_buildtools",
        sha256 = "67e0d6b79724f413b98cebfa256db30cede6f9f14aae1ac50d0987da702f5054",
        strip_prefix = "buildtools-6.3.3",
        urls = [
            "https://github.com/bazelbuild/buildtools/archive/refs/tags/v6.3.3.zip",
        ],
    )
