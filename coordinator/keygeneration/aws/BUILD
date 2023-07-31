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
load(":build_defs.bzl", "keygeneration_ami")

package(default_visibility = ["//visibility:public"])

# Default AMI name used by //coordinator/keygeneration/aws:build_defs.bzl.
string_flag(
    name = "keygeneration_ami_name",
    build_setting_default = "keygeneration_prod",
)

string_flag(
    name = "keygeneration_ami_region",
    build_setting_default = "None",
)

keygeneration_ami(
    name = "prod_keygen_ami",
    aws_region_override = ":keygeneration_ami_region",
)
