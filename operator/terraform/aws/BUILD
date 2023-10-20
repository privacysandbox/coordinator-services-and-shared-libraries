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
load(":build_defs.bzl", "ami_params_tfvar_file")

package(default_visibility = ["//visibility:public"])

# Shared ami_params.auto.tfvars value which contains the values configured the build flags.
ami_params_tfvar_file(
    name = "ami_params_auto_tfvars",
    file_name = "ami_params.auto.tfvars",
)

pkg_files(
    name = "dist_jars",
    srcs = [
        "//java/com/google/scp/operator/autoscaling/app/aws:AsgCapacityHandlerLambdaDeploy",
        "//java/com/google/scp/operator/autoscaling/app/aws:TerminatedInstanceHandlerLambdaDeploy",
        "//java/com/google/scp/operator/frontend:AwsApiGatewayFrontendDeployPackage",
        "//java/com/google/scp/operator/frontend/service/aws:AwsChangeHandlerLambdaDeploy",
        "//java/com/google/scp/operator/frontend/service/aws:AwsFrontendCleanupLambdaDeploy",
    ],
    prefix = "jars",
)

pkg_tar(
    name = "operator_tar",
    srcs = [
        ":dist_jars",
        "//licenses",
        "//operator/aws/docs",
        "//operator/terraform/aws/applications:applications_pkg",
        "//operator/terraform/aws/environments/demo:demo_environment_pkg",
        "//operator/terraform/aws/modules:modules_pkg",
    ],
    extension = "tgz",
    strip_prefix = "/",
)
