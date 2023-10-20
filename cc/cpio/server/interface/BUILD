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

load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")
load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//cc:scp_internal_pkg"])

# Flag to determine if we run test server.
bool_flag(
    name = "is_test_server",
    build_setting_default = False,
)

config_setting(
    name = "aws_server",
    flag_values = {
        "//cc/public/cpio/interface:platform": "aws",
        ":is_test_server": "False",
    },
)

config_setting(
    name = "aws_server_inside_tee",
    flag_values = {
        "//cc/public/cpio/interface:platform": "aws",
        "//cc/public/cpio/interface:run_inside_tee": "True",
        ":is_test_server": "False",
    },
)

config_setting(
    name = "aws_server_outside_tee",
    flag_values = {
        "//cc/public/cpio/interface:platform": "aws",
        "//cc/public/cpio/interface:run_inside_tee": "False",
        ":is_test_server": "False",
    },
)

config_setting(
    name = "gcp_server",
    flag_values = {
        "//cc/public/cpio/interface:platform": "gcp",
        ":is_test_server": "False",
    },
)

config_setting(
    name = "gcp_server_inside_tee",
    flag_values = {
        "//cc/public/cpio/interface:platform": "gcp",
        "//cc/public/cpio/interface:run_inside_tee": "True",
        ":is_test_server": "False",
    },
)

config_setting(
    name = "gcp_server_outside_tee",
    flag_values = {
        "//cc/public/cpio/interface:platform": "gcp",
        "//cc/public/cpio/interface:run_inside_tee": "False",
        ":is_test_server": "False",
    },
)

config_setting(
    name = "test_aws_server",
    flag_values = {
        "//cc/public/cpio/interface:platform": "aws",
        ":is_test_server": "True",
    },
)

config_setting(
    name = "test_gcp_server",
    flag_values = {
        "//cc/public/cpio/interface:platform": "gcp",
        ":is_test_server": "True",
    },
)

cc_library(
    name = "server_interface_lib",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
    ),
    copts = [
        "-std=c++17",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/public/cpio/interface:type_def",
    ],
)
