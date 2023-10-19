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

def boringssl():
    maybe(
        git_repository,
        name = "boringssl",
        # Committed on Oct 3, 2022
        # https://github.com/google/boringssl/commit/c2837229f381f5fcd8894f0cca792a94b557ac52
        commit = "c2837229f381f5fcd8894f0cca792a94b557ac52",
        remote = "https://github.com/google/boringssl.git",
    )
