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
load("@rules_foreign_cc//foreign_cc:defs.bzl", "configure_make")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "gperftools_srcs",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

# You can link this library to your cc_binary or cc_test so that you can get
# CPU profile when running the the binary or test. To link this library to
# your cc_binary or cc_test, use the linkopts argument, for example:
#
#   cc_test(
#       name = "my_test",
#       linkopts = ["-lprofiler"],
#       deps = ["@gperftools"],
#   )
#
# To get the CPU profile, you must set the CPUPROFILE environment variable
# before running your binary or test, for example:
#
#   $ bazel build //cc/core/async_executor/test:async_executor_test
#   $ CPUPROFILE=/tmp/prof.out ./bazel-bin/cc/core/async_executor/test/async_executor_test
#
# The CPU profile will be stored in the file path specified by CPUPROFILE
# environment variable. The explore the CPU profile, you need to install the
# pprof CLI (https://github.com/google/pprof). Here is an example:
#
#   $ pprof -top \
#       ./bazel-bin/cc/core/async_executor/test/async_executor_test \
#       /tmp/prof.out
configure_make(
    name = "gperftools",
    configure_in_place = True,
    lib_source = ":gperftools_srcs",
    out_shared_libs = [
        "libprofiler.so",
        "libprofiler.so.0",
    ],
)
