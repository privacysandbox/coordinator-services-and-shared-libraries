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

# Default protobuf version and hash
DEFAULT_PROTOBUF_CORE_VERSION = "25.3"
DEFAULT_PROTOBUF_SHA_256 = "d19643d265b978383352b3143f04c0641eea75a75235c111cc01a1350173180e"

def protobuf(version = DEFAULT_PROTOBUF_CORE_VERSION, hash = DEFAULT_PROTOBUF_SHA_256):
    maybe(
        http_archive,
        name = "com_google_protobuf",
        sha256 = hash,
        strip_prefix = "protobuf-%s" % version,
        urls = [
            "https://github.com/protocolbuffers/protobuf/archive/v%s.tar.gz" % version,
        ],
    )
