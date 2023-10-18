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

def bazel_rules_java():
    maybe(
        http_archive,
        name = "rules_java",
        sha256 = "27abf8d2b26f4572ba4112ae8eb4439513615018e03a299f85a8460f6992f6a3",
        url = "https://github.com/bazelbuild/rules_java/releases/download/6.4.0/rules_java-6.4.0.tar.gz",
    )
