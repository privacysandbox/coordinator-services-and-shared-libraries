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

def gperftools():
    maybe(
        http_archive,
        name = "gperftools",
        sha256 = "c69fef855628c81ef56f12e3c58f2b7ce1f326c0a1fe783e5cae0b88cbbe9a80",
        strip_prefix = "gperftools-2.15",
        urls = [
            "https://github.com/gperftools/gperftools/releases/download/gperftools-2.15/gperftools-2.15.tar.gz",
        ],
        build_file = Label("//build_defs/cc/shared/build_targets:gperftools.BUILD"),
    )
