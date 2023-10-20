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

load("@rules_cc//cc:defs.bzl", "cc_test")

package(default_visibility = ["//cc:scp_internal_pkg"])

cc_test(
    name = "gcp_utils_test",
    size = "small",
    srcs = glob(
        [
            "*.cc",
        ],
    ),
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/cpio/common/src/gcp:gcp_utils_lib",
        "//cc/public/core/test/interface:execution_result_matchers",
        "//cc/public/cpio/interface:cpio_errors",
        "@com_github_googleapis_google_cloud_cpp//:common",
        "@com_google_googletest//:gtest_main",
    ],
)
