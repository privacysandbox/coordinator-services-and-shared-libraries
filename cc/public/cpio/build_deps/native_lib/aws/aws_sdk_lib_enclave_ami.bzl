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

load("//build_defs/aws/enclave:enclave_ami.bzl", "generic_enclave_ami_pkr_script")
load("//build_defs/shared:packer_build.bzl", "packer_build")
load("//cc/public/cpio/build_deps/shared:sdk_container.bzl", "sdk_container")

_LICENSES_TARGET = Label("//licenses:licenses_tar")

def aws_sdk_lib_enclave_ami(
        *,
        name,
        inside_tee,
        client_binaries = {},
        recover_client_binaries = True,
        additional_env_variables = {},
        additional_files = [],
        additional_tars = [],
        pkgs_to_install = [],
        sdk_cmd_override = [],
        ami_name = Label("//cc/public/cpio/build_deps/native_lib/aws:aws_sdk_lib_enclave_ami_name"),
        aws_region_override = Label("//cc/public/cpio/build_deps/native_lib/aws:aws_region_override"),
        debug_mode = False):
    """
    Creates a runnable target for deploying a AWS SDK lib enclave AMI.

    To deploy an AMI, `bazel run` the provided name of this target.
    The generated Packer script is available as `$name.pkr.hcl`
    """
    sdk_container(
        name = name,
        client_binaries = client_binaries,
        additional_env_variables = additional_env_variables,
        inside_tee = inside_tee,
        platform = "aws",
        recover_client_binaries = recover_client_binaries,
        additional_files = additional_files,
        additional_tars = additional_tars,
        pkgs_to_install = pkgs_to_install,
        sdk_cmd_override = sdk_cmd_override,
    )

    packer_script_name = "%s.pkr.hcl" % name
    generic_enclave_ami_pkr_script(
        name = packer_script_name,
        ami_name = ami_name,
        aws_region_override = aws_region_override,
        # EC2 instance type used to build the AMI.
        ec2_instance = "m5.xlarge",
        enable_enclave_debug_mode = debug_mode,
        enclave_allocator = Label("//build_defs/aws/enclave:small.allocator.yaml"),
        enclave_container_image = ":%s.tar" % reproducible_container_name,
        enclave_container_tag = container_name,
        licenses = _LICENSES_TARGET,
        tags = ["manual"],
    )

    packer_build(
        name = name,
        packer_file = ":%s" % packer_script_name,
        tags = ["manual"],
    )
