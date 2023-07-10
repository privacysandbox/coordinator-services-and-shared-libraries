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

load("//build_defs:release.bzl", "gcs_package_release")

package(default_visibility = ["//visibility:public"])

gcs_package_release(
    name = "operator_aws_release",
    artifact_base_name = "scp-operator-{VERSION}.tgz",
    package_target = "//operator/terraform/aws:operator_tar",
    release_bucket = "admcloud-scp-repo-dev",
    release_key = "operator-dms",
)
