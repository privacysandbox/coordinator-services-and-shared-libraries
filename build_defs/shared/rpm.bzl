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
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def rpm():
    maybe(
        git_repository,
        name = "com_github_google_rpmpack",
        # Lastest commit in main branch as of 2021-11-29
        commit = "d0ed9b1b61b95992d3c4e83df3e997f3538a7b6c",
        remote = "https://github.com/google/rpmpack.git",
        shallow_since = "1637822718 +0200",
    )
