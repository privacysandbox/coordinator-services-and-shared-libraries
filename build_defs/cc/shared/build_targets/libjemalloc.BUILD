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
load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "jemalloc_srcs",
    srcs = glob(["jemalloc-5.3.0/**"]),
)

# https://bazelbuild.github.io/rules_foreign_cc/main/configure_make.html#configure_make-autogen
configure_make(
    name = "libjemalloc",
    autogen = True,
    configure_in_place = True,
    lib_source = ":jemalloc_srcs",
)

cc_library(
    name = "libjemalloc_static",
    linkopts = [
        "-lm",
        "-lstdc++",
        "-pthread",
    ],
    linkstatic = 1,
    deps = ["@jemalloc//:libjemalloc"],
    alwayslink = 1,
)
