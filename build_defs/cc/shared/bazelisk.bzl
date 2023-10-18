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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

# Note: these rules add a dependency on the golang toolchain and must be ordered
# after any `go_register_toolchains` calls in this file (or else the toolchain
# defined in io_bazel_rules_docker are used for future go toolchains)

def bazelisk():
    # Needed for build containers which must execute bazel commands (e.g. //cc/aws/proxy).
    maybe(
        http_file,
        name = "bazelisk",
        downloaded_file_path = "bazelisk",
        executable = True,
        sha256 = "84e946ed8537eaaa4d540df338a593e373e70c5ddca9f2f49e1aaf3a04bdd6ca",
        urls = ["https://github.com/bazelbuild/bazelisk/releases/download/v1.14.0/bazelisk-linux-amd64"],
    )
