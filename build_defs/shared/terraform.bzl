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

def terraform():
    maybe(
        http_archive,
        name = "terraform",
        build_file_content = """
package(default_visibility = ["//visibility:public"])
exports_files(["terraform"])
""",
        sha256 = "dcaf8ba801660a431a6769ec44ba53b66c1ad44637512ef3961f7ffe4397ef7c",
        url = "https://releases.hashicorp.com/terraform/1.12.1/terraform_1.12.1_linux_amd64.zip",
    )
