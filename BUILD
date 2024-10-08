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

load("@bazel_gazelle//:def.bzl", "gazelle")
load("@bazel_skylib//rules:copy_directory.bzl", "copy_directory")
load("@com_github_bazelbuild_buildtools//buildifier:def.bzl", "buildifier")
load("@rules_pkg//:mappings.bzl", "pkg_files")
load("@rules_pkg//:pkg.bzl", "pkg_tar")

package(default_visibility = ["//visibility:public"])

# Gazelle is used to import target and create build rule automatically. It supports many
# languages but this repo only uses it for Golang. To use this tool, please run:
#
#   bazel run //go:gazelle
#
# The following directive asks gazelle to only process Golang codes
# gazelle:lang go
#
# gazelle:prefix github.com/privacysandbox/coordinator-services-and-shared-libraries
gazelle(name = "gazelle")

# The following build rule can be used to update WORKSPACE file to include Golang
# dependency.
#
# gazelle(
#     name = "gazelle-update-repos",
#     args = [
#         "-from_file=go/helloworld/go.mod",
#         "-to_macro=./build_defs/go/go_repositories.bzl%go_dependencies",
#         "-prune",
#     ],
#     command = "update-repos",
# )

buildifier(
    name = "buildifier_check",
    mode = "check",
)

buildifier(
    name = "buildifier_fix",
    lint_mode = "fix",
    mode = "fix",
)

# pkg_tar no longer allows directories to be specified.
# Must use copy_directory to create Tree Artifacts.
# https://github.com/bazelbuild/rules_pkg/issues/611
#
# The srcs directory is prefixed to avoid the error conflicting with
# other build rules:
# "One of the output paths ... is a prefix of the other.
# These actions cannot be simultaneously present;
# please rename one of the output files or build just one of them"
# It will be stripped by pkg_tar remap_paths.

copy_directory(
    name = "build_defs_dir",
    src = "build_defs",
    out = "srcs/build_defs",
)

copy_directory(
    name = "cc_dir",
    src = "cc",
    out = "srcs/cc",
)

copy_directory(
    name = "java_dir",
    src = "java",
    out = "srcs/java",
)

copy_directory(
    name = "javatests_dir",
    src = "javatests",
    out = "srcs/javatests",
)

copy_directory(
    name = "licenses_dir",
    src = "licenses",
    out = "srcs/licenses",
)

copy_directory(
    name = "operator_dir",
    src = "operator",
    out = "srcs/operator",
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
        ":build_defs_dir",
        ":cc_dir",
        ":java_dir",
        ":javatests_dir",
        ":licenses_dir",
        ":operator_dir",
    ] + glob(["*.bzl"]),
    mode = "0777",
    package_dir = "scp",
    remap_paths = {
        "srcs/": "",
    },
)

pkg_files(
    name = "version_file",
    srcs = ["version.txt"],
)

genrule(
    name = "generate_version_file",
    srcs = ["version.txt"],
    outs = ["version_for_image.txt"],
    cmd = "cp $< $@",
)
