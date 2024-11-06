# Copyright 2024 Google LLC
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

# See instructions at
# https://github.com/bazelbuild/platforms

# 0.0.10
# https://github.com/bazelbuild/platforms/releases/
DEFAULT_PLATFORMS_VERSION = "0.0.10"
DEFAULT_PLATFORMS_SHA_256 = "218efe8ee736d26a3572663b374a253c012b716d8af0c07e842e82f238a0a7ee"

# Loads platforms
def platforms(cpp_version = DEFAULT_PLATFORMS_VERSION, cpp_hash = DEFAULT_PLATFORMS_SHA_256):
    maybe(
        http_archive,
        name = "platforms",
        sha256 = cpp_hash,
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.10/platforms-0.0.10.tar.gz",
            "https://github.com/bazelbuild/platforms/releases/download/0.0.10/platforms-0.0.10.tar.gz",
        ],
    )
