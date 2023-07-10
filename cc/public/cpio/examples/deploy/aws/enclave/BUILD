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

load("@bazel_skylib//rules:common_settings.bzl", "string_flag")
load(":cpio_test_ami.bzl", "cpio_test_ami")

package(default_visibility = ["//cc:scp_internal_pkg"])

# Default AMI name used by cpio_test_ami.bzl.
string_flag(
    name = "cpio_test_ami_name",
    build_setting_default = "cpio_test",
)

cpio_test_ami(
    name = "cpio_test",
    binary_filename = "metric_client_test",
    binary_target = "//cc/public/cpio/examples:metric_client_test",
)
