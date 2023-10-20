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

def aws_c_common():
    maybe(
        http_archive,
        name = "aws_c_common",
        build_file = Label("//build_defs/cc/aws:aws_c_common_source_code.BUILD"),
        sha256 = "462dc0189a83aeb3973002d17586eb19a4dde7d8a4cb541be3113d6a885d64b4",
        strip_prefix = "aws-c-common-0.8.16",
        urls = [
            "https://github.com/awslabs/aws-c-common/archive/refs/tags/v0.8.16.tar.gz",
        ],
    )
