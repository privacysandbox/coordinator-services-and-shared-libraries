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

load("@com_github_bazelbuild_buildtools//buildifier:def.bzl", "buildifier")
load("@rules_pkg//:pkg.bzl", "pkg_tar")

package(default_visibility = ["//visibility:public"])

buildifier(
    name = "buildifier_check",
    mode = "check",
)

buildifier(
    name = "buildifier_fix",
    lint_mode = "fix",
    mode = "fix",
)

# This rule is used to copy the source code from other bazel rules.
# This can be used for reproducible builds.
# Only cc targets are needed at this point, so only the files needed to build
# cc targets are copied.
pkg_tar(
    name = "source_code_tar",
    srcs = [
        ".bazelrc",
        ".bazelversion",
        "BUILD",
        "WORKSPACE",
        "build_defs",
        "cc",
        "java",
        "javatests",
        "licenses",
        "operator",
    ] + glob(["*.bzl"]),
    mode = "0777",
    package_dir = "scp",
)
