# Copyright 2023 Google LLC
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

def libpsl():
    _ALL_CONTENT = """\
filegroup(
    name = "all_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)
"""
    maybe(
        http_archive,
        name = "libpsl",
        build_file_content = _ALL_CONTENT,
        sha256 = "e35991b6e17001afa2c0ca3b10c357650602b92596209b7492802f3768a6285f",
        strip_prefix = "libpsl-0.21.2",
        urls = [
            "https://github.com/rockdaboot/libpsl/releases/download/0.21.2/libpsl-0.21.2.tar.gz",
        ],
    )
