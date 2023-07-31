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

load("pbs_container_multi_stage_build_tools.bzl", "load_pbs_container_multi_stage_container_build_tools")

package(default_visibility = ["//visibility:public"])

load_pbs_container_multi_stage_container_build_tools()

#-------------------------------
# Generate PBS Container for AWS
#-------------------------------
# This rule can be used to build the PBS container image in a reproducible manner.
# It builds the image within a container with fixed libraries and dependencies.
genrule(
    name = "reproducible_pbs_container_aws",
    srcs = [
        "build_reproducible_container_image.sh",
        "//:source_code_tar",
        "//cc/tools/build:prebuilt_cc_build_container_image.tar",
    ],
    outs = ["reproducible_pbs_container_aws.tar"],
    # NOTE: This order matters
    # Arguments:
    # $1 is the output tar, that is, the path where this rule generates its output ($@)
    # $2 is the packaged SCP source code ($(location //:source_code_tar))
    # $3 is the build type
    # $4 is the build container image TAR path
    cmd = "./$(location build_reproducible_container_image.sh) $@ $(location //:source_code_tar) aws $(location //cc/tools/build:prebuilt_cc_build_container_image.tar)",
    tags = ["manual"],
)

#-------------------------------
# Generate PBS Container for GCP
#-------------------------------
# This rule can be used to build the PBS container image in a reproducible manner.
# It builds the image within a container with fixed libraries and dependencies.
genrule(
    name = "reproducible_pbs_container_gcp",
    srcs = [
        "build_reproducible_container_image.sh",
        "//:source_code_tar",
        "//cc/tools/build:prebuilt_cc_build_container_image.tar",
    ],
    outs = ["reproducible_pbs_container_gcp.tar"],
    # NOTE: This order matters
    # Arguments:
    # $1 is the output tar, that is, the path where this rule generates its output ($@)
    # $2 is the packaged SCP source code ($(location //:source_code_tar))
    # $3 is the build type
    # $4 is the build container image TAR path
    cmd = "./$(location build_reproducible_container_image.sh) $@ $(location //:source_code_tar) gcp $(location //cc/tools/build:prebuilt_cc_build_container_image.tar)",
    tags = ["manual"],
)

#------------------------------------------------
# Generate PBS Container for AWS Integration Test
#------------------------------------------------
# This rule can be used to build the PBS container image in a reproducible manner.
# It builds the image for testing within a container with fixed libraries and dependencies.
genrule(
    name = "pbs_container_for_aws_integration_test",
    srcs = [
        "build_reproducible_container_image.sh",
        "//:source_code_tar",
        "//cc/tools/build:prebuilt_cc_build_container_image.tar",
    ],
    outs = ["pbs_container_for_aws_integration_test.tar"],
    # NOTE: This order matters
    # Arguments:
    # $1 is the output tar, that is, the path where this rule generates its output ($@)
    # $2 is the packaged SCP source code ($(location //:source_code_tar))
    # $3 is the PBS build type
    # $4 is the build container image TAR path
    cmd = "./$(location build_reproducible_container_image.sh) $@ $(location //:source_code_tar) aws_integration_test $(location //cc/tools/build:prebuilt_cc_build_container_image.tar)",
    tags = ["manual"],
)

#------------------------------------------------
# Generate PBS Container for GCP Integration Test
#------------------------------------------------
# This rule can be used to build the PBS container image in a reproducible manner.
# It builds the image for testing within a container with fixed libraries and dependencies.
genrule(
    name = "pbs_container_for_gcp_integration_test",
    srcs = [
        "build_reproducible_container_image.sh",
        "//:source_code_tar",
        "//cc/tools/build:prebuilt_cc_build_container_image.tar",
    ],
    outs = ["pbs_container_for_gcp_integration_test.tar"],
    # NOTE: This order matters
    # Arguments:
    # $1 is the output tar, that is, the path where this rule generates its output ($@)
    # $2 is the packaged SCP source code ($(location //:source_code_tar))
    # $3 is the PBS build type
    # $4 is the build container image TAR path
    cmd = "./$(location build_reproducible_container_image.sh) $@ $(location //:source_code_tar) gcp_integration_test $(location //cc/tools/build:prebuilt_cc_build_container_image.tar)",
    tags = ["manual"],
)
