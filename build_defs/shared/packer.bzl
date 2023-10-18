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

def packer():
    maybe(
        http_archive,
        name = "packer",
        build_file_content = """
package(default_visibility = ["//visibility:public"])
exports_files(["packer"])
""",
        sha256 = "57d0411e578aea62918d36ed186951139d5d49d44b76e5666d1fbf2427b385ae",
        url = "https://releases.hashicorp.com/packer/1.8.6/packer_1.8.6_linux_amd64.zip",
    )
