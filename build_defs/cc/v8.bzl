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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("@rules_python//python:pip.bzl", "pip_install")

def import_v8(scp_repo_name = ""):
    maybe(
        git_repository,
        name = "v8",
        # tag 11.1.277.9
        commit = "3e5cb8693c290fa0f60eb92aad84d09e68507678",
        remote = "https://chromium.googlesource.com/v8/v8.git",
        shallow_since = "1677006717 +0000",
        patch_args = ["-p1"],
        patches = [scp_repo_name + "//build_defs/cc:v8.patch"],
    )

    pip_install(
        name = "v8_python_deps",
        extra_pip_args = ["--require-hashes"],
        requirements = "@v8//:bazel/requirements.txt",
    )

    new_git_repository(
        name = "com_googlesource_chromium_icu",
        build_file = "@v8//:bazel/BUILD.icu",
        commit = "7ff1e9befce5567754dc88392dfaa4704e261ab3",
        remote = "https://chromium.googlesource.com/chromium/deps/icu.git",
        shallow_since = "1676660986 +0000",
        patch_cmds = ["find source -name BUILD.bazel | xargs rm"],
    )

    native.bind(
        name = "icu",
        actual = "@com_googlesource_chromium_icu//:icu",
    )

    native.bind(
        name = "zlib_compression_utils",
        actual = "@com_googlesource_chromium_zlib//:zlib_compression_utils",
    )

    new_git_repository(
        name = "com_googlesource_chromium_base_trace_event_common",
        build_file = "@v8//:bazel/BUILD.trace_event_common",
        commit = "521ac34ebd795939c7e16b37d9d3ddb40e8ed556",
        remote = "https://chromium.googlesource.com/chromium/src/base/trace_event/common.git",
        shallow_since = "1662669790 -0700",
    )

    native.bind(
        name = "base_trace_event_common",
        actual = "@com_googlesource_chromium_base_trace_event_common//:trace_event_common",
    )
