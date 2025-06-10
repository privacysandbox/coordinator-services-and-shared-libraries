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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//cc:pbs_visibility"])

# Copy the file in the test directory for simplier executable file path
genrule(
    name = "docker_compose_executable",
    srcs = ["@docker_compose//file"],
    outs = ["docker-compose"],
    cmd = "cp $(location @docker_compose//file) $@",
    tags = ["manual"],
)

cc_library(
    name = "docker_helper_lib",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
        exclude = [
            "docker_helper_test.cc",
        ],
    ),
    data = [
        ":docker_compose_executable",
    ],
    deps = [
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "docker_helper_test",
    size = "small",
    srcs = ["docker_helper_test.cc"],
    deps = [
        ":docker_helper_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
