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

def google_api_core():
    maybe(
        http_archive,
        name = "google_api_core",
        sha256 = "fa65e42bcafdb5cf25d019ef2fbb612fd4268378e0cfdd333622fdeea353134f",
        strip_prefix = "python-api-core-2.19.1",
        url = "https://github.com/googleapis/python-api-core/archive/refs/tags/v2.19.1.tar.gz",
    )
