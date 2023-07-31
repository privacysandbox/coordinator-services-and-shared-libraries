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

def import_aws_nitro_enclaves_sdk_c(repo_name = ""):
    http_archive(
        name = "aws_nitro_enclaves_sdk_c",
        build_file = Label("//build_defs/aws/kmstool:kmstool.BUILD"),
        patches = [Label("//build_defs/aws/kmstool:kmstool.patch")],
        patch_args = ["-p1"],
        sha256 = "bc937626e1058c2464e60dde3a410855b87987e6da23433d78e77aedc8a152ec",
        strip_prefix = "aws-nitro-enclaves-sdk-c-e3425251b5fd573a730101b091f770ad21b9ee56",
        urls = ["https://github.com/aws/aws-nitro-enclaves-sdk-c/archive/e3425251b5fd573a730101b091f770ad21b9ee56.zip"],
    )
