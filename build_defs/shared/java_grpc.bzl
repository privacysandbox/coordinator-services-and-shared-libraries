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

GAPIC_GENERATOR_JAVA_VERSION = "2.19.0"

def java_grpc():
    maybe(
        http_archive,
        name = "gapic_generator_java",
        strip_prefix = "sdk-platform-java-%s" % GAPIC_GENERATOR_JAVA_VERSION,
        urls = ["https://github.com/googleapis/sdk-platform-java/archive/v%s.zip" % GAPIC_GENERATOR_JAVA_VERSION],
    )

    # gax-java is part of sdk-platform-java repository
    maybe(
        http_archive,
        name = "com_google_api_gax_java",
        strip_prefix = "sdk-platform-java-%s/gax-java" % GAPIC_GENERATOR_JAVA_VERSION,
        urls = ["https://github.com/googleapis/sdk-platform-java/archive/v%s.zip" % GAPIC_GENERATOR_JAVA_VERSION],
    )
