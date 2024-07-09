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

DEFAULT_BAZEL_RULES_PKG_VERSION = "1.0.0"
DEFAULT_BAZEL_RULES_PKG_VERSION_SHA_256 = "cad05f864a32799f6f9022891de91ac78f30e0fa07dc68abac92a628121b5b11"

def bazel_rules_pkg(version = DEFAULT_BAZEL_RULES_PKG_VERSION, hash = DEFAULT_BAZEL_RULES_PKG_VERSION_SHA_256):
    maybe(
        http_archive,
        name = "rules_pkg",
        sha256 = hash,
        urls = [
            # 1.0.0 has not been uploaded to the mirror yet?
            # "https://mirror.bazel.build/github.com/bazelbuild/rules_pkg/releases/download/%s/rules_pkg-%s.tar.gz" % (version, version),
            "https://github.com/bazelbuild/rules_pkg/releases/download/%s/rules_pkg-%s.tar.gz" % (version, version),
        ],
    )
