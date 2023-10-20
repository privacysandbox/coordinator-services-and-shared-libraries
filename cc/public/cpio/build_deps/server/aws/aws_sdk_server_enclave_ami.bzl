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
load("//cc/public/cpio/build_deps/server:cmrt_sdk.bzl", "cmrt_sdk")

_LICENSES_TARGET = Label("//licenses:licenses_tar")

def aws_sdk_server_enclave_ami(
        name,
        client_binaries,
        ami_name = Label("//cc/public/cpio/build_deps/server/aws:aws_sdk_server_enclave_ami_name"),
        aws_region = "us-east-1",
        job_service_configs = {},
        private_key_service_configs = {},
        public_key_service_configs = {},
        queue_service_configs = {},
        debug_mode = False):
    """
    Creates a runnable target for deploying a SDK test AMI.

    To deploy an AMI, `bazel run` the provided name of this target.
    The generated Packer script is available as `$name.pkr.hcl`
    """
    cmrt_sdk(
        name = name,
        platform = "aws",
        inside_tee = True,
        client_binaries = client_binaries,
        job_service_configs = job_service_configs,
        private_key_service_configs = private_key_service_configs,
        public_key_service_configs = public_key_service_configs,
        queue_service_configs = queue_service_configs,
    )

    container_name = "%s_container" % name
    reproducible_container_name = "%s_reproducible_container" % name
    packer_script_name = "%s.pkr.hcl" % name
    generic_enclave_ami_pkr_script(
        name = packer_script_name,
        ami_name = ami_name,
        aws_region = aws_region,
        # EC2 instance type used to build the AMI.
        ec2_instance = "m5.xlarge",
        subnet_id = "",
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
