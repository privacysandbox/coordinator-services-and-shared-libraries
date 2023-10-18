# Copyright 2023 Google LLC
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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def sandboxed_api(scp_repo_name = ""):
    maybe(
        git_repository,
        name = "com_google_sandboxed_api",
        # main as of 08-24-2023
        commit = "d5ff466c59993553fe546776488b86e9bb794b5c",
        remote = "https://github.com/google/sandboxed-api.git",
        patch_args = ["-p1"],
        patches = [scp_repo_name + "//build_defs/cc/shared:sandboxed_api.patch"],
        shallow_since = "1692892706 -0700",
    )
