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

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_library(
    name = "core_test_lib",
    srcs = glob(
        [
            "*.h",
        ],
    ),
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
    ],
)

cc_test(
    name = "service_interface_test",
    size = "small",
    srcs = ["service_interface_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "errors_test",
    size = "small",
    srcs = ["errors_test.cc"],
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/common/concurrent_queue/src:concurrent_queue_lib",
        "//cc/core/interface:interface_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "type_def_test",
    srcs = ["type_def_test.cc"],
    deps = [
        "//cc/core/interface:type_def_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
