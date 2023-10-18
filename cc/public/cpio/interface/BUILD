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

load("@bazel_skylib//rules:common_settings.bzl", "bool_flag", "string_flag")
load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

# Flag to determine which platform the system will be built and run on.
# Example:
# bazel build target --//cc/public/cpio/interface:platform=aws
string_flag(
    name = "platform",
    build_setting_default = "aws",
    values = [
        "aws",
        "gcp",
    ],
)

config_setting(
    name = "aws_cpio_lib",
    flag_values = {
        ":platform": "aws",
    },
)

config_setting(
    name = "gcp_cpio_lib",
    flag_values = {
        ":platform": "gcp",
    },
)

# Flag to determine if we run code inside TEE.
bool_flag(
    name = "run_inside_tee",
    build_setting_default = True,
)

config_setting(
    name = "aws_cpio_lib_inside_tee",
    flag_values = {
        ":platform": "aws",
        ":run_inside_tee": "True",
    },
)

config_setting(
    name = "gcp_cpio_lib_inside_tee",
    flag_values = {
        ":platform": "gcp",
        ":run_inside_tee": "True",
    },
)

config_setting(
    name = "aws_cpio_lib_outside_tee",
    flag_values = {
        ":platform": "aws",
        ":run_inside_tee": "False",
    },
)

config_setting(
    name = "gcp_cpio_lib_outside_tee",
    flag_values = {
        ":platform": "gcp",
        ":run_inside_tee": "False",
    },
)

cc_library(
    name = "type_def",
    hdrs =
        [
            "type_def.h",
        ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
    ],
)

cc_library(
    name = "cpio",
    srcs = ["//cc/public/cpio/core/src:cpio.cc"],
    hdrs = [
        "cpio.h",
    ],
    deps = [
        ":type_def",
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
        "//cc/core/logger/src:logger_lib",
        "//cc/core/logger/src/log_providers:log_providers_lib",
        "//cc/core/logger/src/log_providers/syslog:syslog_lib",
        "//cc/cpio/client_providers/global_cpio/src:global_cpio_lib",
        "//cc/cpio/client_providers/global_cpio/src/cpio_provider:cpio_provider_lib",
        "//cc/public/cpio/core/src:cpio_utils_lib",
    ],
)

exports_files(["cpio.h"])

cc_library(
    name = "cpio_errors",
    hdrs = [
        "error_codes.h",
    ],
    deps = [
        "//cc:cc_base_include_dir",
        "//cc/core/interface:interface_lib",
    ],
)
