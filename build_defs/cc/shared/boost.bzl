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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

# Boost
# latest as of 2022-06-09
RULES_BOOST_COMMIT = "789a047e61c0292c3b989514f5ca18a9945b0029"

def boost():
    maybe(
        git_repository,
        name = "com_github_nelhage_rules_boost",
        commit = RULES_BOOST_COMMIT,
        patch_args = ["-p1"],
        patches = [Label("//build_defs/cc/shared:rules_boost.patch")],
        remote = "https://github.com/nelhage/rules_boost.git",
        shallow_since = "1652895814 -0700",
    )
