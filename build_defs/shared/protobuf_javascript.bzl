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

# The last release from 2022, 3.21.2, is not compatible with the latest
# versions of Protobuf. Use a version from main branch with this pull request:
# https://github.com/protocolbuffers/protobuf-javascript/pull/196

def protobuf_javascript():
    maybe(
        http_archive,
        name = "com_google_protobuf_javascript",
        sha256 = "ddb5c794631fc158b14742247c2d2bb1816bbc628980cd45cd4b678c4472fe2f",
        strip_prefix = "protobuf-javascript-e66d4eb8ef56047b707f889e8e4a3e9769c6f0f8",
        urls = ["https://github.com/protocolbuffers/protobuf-javascript/archive/e66d4eb8ef56047b707f889e8e4a3e9769c6f0f8.tar.gz"],
    )
