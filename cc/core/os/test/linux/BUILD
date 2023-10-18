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

load("@rules_cc//cc:defs.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "system_resource_info_provider_linux_test",
    size = "small",
    srcs = ["system_resource_info_provider_linux_test.cc"],
    copts = [
        "-std=c++17",
    ],
    data = [
        "files/invalid_format_meminfo_file.txt",
        "files/missing_available_meminfo_file.txt",
        "files/valid_meminfo_file.txt",
    ],
    deps = [
        "//cc/core/os/src/linux:system_resource_info_provider_linux_lib",
        "//cc/core/test/utils:utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
