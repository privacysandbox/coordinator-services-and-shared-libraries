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

package(default_visibility = ["//visibility:public"])

package_group(
    name = "pbs_visibility",
    packages = [
        "//cc/core/...",
        "//cc/cpio/...",
        "//cc/pbs/...",
        "//cc/process_launcher/...",
        "//cc/public/...",
    ],
)

string_flag(
    name = "platform",
    build_setting_default = "local",
    values = [
        "gcp",
        "local",
    ],
)

config_setting(
    name = "cloud_platform_gcp",
    flag_values = {"platform": "gcp"},
)

config_setting(
    name = "cloud_platform_local",
    flag_values = {"platform": "local"},
)

bool_flag(
    name = "enable_benchmarking",
    build_setting_default = False,
)

config_setting(
    name = "disable_benchmarking_setting",
    flag_values = {":enable_benchmarking": "False"},
)

config_setting(
    name = "enable_benchmarking_setting",
    flag_values = {":enable_benchmarking": "True"},
)
