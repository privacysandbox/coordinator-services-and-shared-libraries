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
# https://github.com/open-telemetry/opentelemetry-cpp/blob/main/INSTALL.md#incorporating-into-an-existing-bazel-project

# v1.13.0 release
# https://github.com/open-telemetry/opentelemetry-cpp/releases/tag/v1.13.0
DEFAULT_OPENTELEMETRY_CPP_VERSION = "1.13.0"
DEFAULT_OPENTELEMETRY_CPP_SHA_256 = "7735cc56507149686e6019e06f588317099d4522480be5f38a2a09ec69af1706"

# Loads io_opentelemetry_cpp
def opentelemetry_cpp(cpp_version = DEFAULT_OPENTELEMETRY_CPP_VERSION, cpp_hash = DEFAULT_OPENTELEMETRY_CPP_SHA_256):
    maybe(
        http_archive,
        name = "io_opentelemetry_cpp",
        sha256 = cpp_hash,
        strip_prefix = "opentelemetry-cpp-%s" % cpp_version,
        urls = [
            "https://github.com/open-telemetry/opentelemetry-cpp/archive/v%s.tar.gz" % cpp_version,
        ],
    )
