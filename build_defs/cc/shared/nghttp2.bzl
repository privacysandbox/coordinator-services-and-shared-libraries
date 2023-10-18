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

def nghttp2():
    maybe(
        http_archive,
        name = "madler_zlib",
        build_file = Label("//build_defs/cc/shared/build_targets:madler_zlib.BUILD"),
        sha256 = "d233fca7cf68db4c16dc5287af61f3cd01ab62495224c66639ca3da537701e42",
        strip_prefix = "zlib-1.2.13",
        urls = [
            "https://github.com/madler/zlib/releases/download/v1.2.13/zlib1213.zip",
        ],
    )
    maybe(
        http_archive,
        name = "libxml2",
        build_file = Label("//build_defs/cc/shared/build_targets:libxml2.BUILD"),
        patches = [Label("//build_defs/cc/shared/build_targets:libxml2.patch")],
        sha256 = "c8d6681e38c56f172892c85ddc0852e1fd4b53b4209e7f4ebf17f7e2eae71d92",
        strip_prefix = "libxml2-2.9.12",
        urls = [
            "http://xmlsoft.org/sources/libxml2-2.9.12.tar.gz",
        ],
    )
    maybe(
        http_archive,
        name = "com_github_nghttp2_nghttp2",
        build_file = Label("//build_defs/cc/shared/build_targets:nghttp2.BUILD"),
        patch_args = ["-p1"],
        patches = [Label("//build_defs/cc/shared/build_targets:nghttp2.patch")],
        sha256 = "62f50f0e9fc479e48b34e1526df8dd2e94136de4c426b7680048181606832b7c",
        strip_prefix = "nghttp2-1.47.0",
        urls = [
            "https://github.com/nghttp2/nghttp2/releases/download/v1.47.0/nghttp2-1.47.0.tar.gz",
        ],
    )
