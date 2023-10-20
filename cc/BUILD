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

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

# This library is to be used as a dummy dependency of all cc targets under this
# directory, so that we don't need to type the full include path containing
# "cc/".
cc_library(
    name = "cc_base_include_dir",
    includes = ["."],
    visibility = [":__subpackages__"],
)

package_group(
    name = "scp_internal_pkg",
    packages = [
        "//cc/...",
    ],
)
