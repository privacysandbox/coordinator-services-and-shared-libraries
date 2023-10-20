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

load("@rules_pkg//:mappings.bzl", "pkg_files")
load("@rules_pkg//:pkg.bzl", "pkg_tar")

package(default_visibility = ["//visibility:public"])

pkg_files(
    name = "dist_jars",
    srcs = [
        "//java/com/google/scp/operator/autoscaling/app/gcp:WorkerScaleInCloudFunctionDeploy",
        "//java/com/google/scp/operator/frontend/service/gcp:FrontendServiceHttpCloudFunctionDeploy",
    ],
    prefix = "jars",
)

pkg_tar(
    name = "operator_tar",
    srcs = [
        ":dist_jars",
        "//operator/terraform/gcp/applications:applications_pkg",
        "//operator/terraform/gcp/environments:operator_tar_environments",
        "//operator/terraform/gcp/modules:modules_pkg",
        "//operator/terraform/gcp/modules/shared:modules_shared_pkg",
        "//operator/terraform/gcp/util_scripts:util_scripts_pkg",
    ],
    extension = "tgz",
    strip_prefix = "/",
)
