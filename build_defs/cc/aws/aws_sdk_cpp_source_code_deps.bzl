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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def import_aws_sdk_cpp():
    """
    Import AWS SDK CPP source code
    """
    maybe(
        http_archive,
        name = "aws_checksums",
        build_file = Label("//build_defs/cc/aws:aws_checksums.BUILD"),
        sha256 = "6e6bed6f75cf54006b6bafb01b3b96df19605572131a2260fddaf0e87949ced0",
        strip_prefix = "aws-checksums-0.1.5",
        urls = [
            "https://github.com/awslabs/aws-checksums/archive/v0.1.5.tar.gz",
        ],
    )

    maybe(
        http_archive,
        name = "aws_c_common",
        build_file = Label("//build_defs/cc/aws:aws_c_common.BUILD"),
        sha256 = "6eb0b806c78b36a32eec9bcba8d2833e3973491a29d46fe3d11edc3f8d3e7f73",
        strip_prefix = "aws-c-common-0.6.20",
        urls = [
            "https://github.com/awslabs/aws-c-common/archive/refs/tags/v0.6.20.tar.gz",
        ],
    )

    maybe(
        http_archive,
        name = "aws_c_event_stream",
        build_file = Label("//build_defs/cc/aws:aws_c_event_stream.BUILD"),
        sha256 = "31d880d1c868d3f3df1e1f4b45e56ac73724a4dc3449d04d47fc0746f6f077b6",
        strip_prefix = "aws-c-event-stream-0.1.4",
        urls = [
            "https://github.com/awslabs/aws-c-event-stream/archive/v0.1.4.tar.gz",
        ],
    )

    maybe(
        http_archive,
        name = "aws_sdk_cpp",
        build_file = Label("//build_defs/cc/aws:aws_sdk_cpp_source_code.BUILD"),
        patch_cmds = [
            # Apply fix in https://github.com/aws/aws-sdk-cpp/commit/9669a1c1d9a96621cd0846679cbe973c648a64b3
            """sed -i.bak 's/Tags\\.entry/Tag/g' aws-cpp-sdk-sqs/source/model/TagQueueRequest.cpp""",
        ],
        sha256 = "749322a8be4594472512df8a21d9338d7181c643a00e08a0ff12f07e831e3346",
        strip_prefix = "aws-sdk-cpp-1.8.186",
        urls = [
            "https://github.com/aws/aws-sdk-cpp/archive/1.8.186.tar.gz",
        ],
    )
