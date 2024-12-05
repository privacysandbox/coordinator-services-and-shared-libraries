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

# See instructions at
# https://github.com/google/re2

# 2024-07-02
# https://github.com/google/re2/releases
DEFAULT_RE2_CPP_VERSION = "2024-07-02"
DEFAULT_RE2_CPP_SHA_256 = "eb2df807c781601c14a260a507a5bb4509be1ee626024cb45acbd57cb9d4032b"

# Loads re2_cpp
def re2_cpp(cpp_version = DEFAULT_RE2_CPP_VERSION, cpp_hash = DEFAULT_RE2_CPP_SHA_256):
    maybe(
        http_archive,
        name = "com_googlesource_code_re2",
        sha256 = cpp_hash,
        strip_prefix = "re2-%s" % cpp_version,
        urls = [
            "https://github.com/google/re2/releases/download/2024-07-02/re2-%s.tar.gz" % cpp_version,
        ],
        repo_mapping = {"@abseil-cpp": "@com_google_absl"},
    )
