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

load("//build_defs/aws/enclave:container.bzl", "java_enclave_image")
load("//build_defs/aws/enclave:enclave_ami.bzl", "generic_enclave_ami_pkr_script")
load("//build_defs/shared:packer_build.bzl", "packer_build")

_LICENSES_TARGET = "//licenses:licenses_tar"

def keygeneration_ami(
        name,
        ami_name = Label("//coordinator/keygeneration/aws:keygeneration_ami_name"),
        aws_region = "us-east-1",
        aws_region_override = None,
        subnet_id = "",
        debug_mode = False,
        uninstall_ssh_server = True,
        additional_jar_args = []):
    """
    Creates a runnable target for deploying a keygeneration AMI.

    To deploy an AMI, `bazel run` the provided name of this target.
    The generated container is available as `$name_keygeneration_container`
    The generated Packer script is available as `$name_kegeneration.pkr.hcl`

    Args:
      uninstall_ssh_server: Whether to uninstall SSH server from AMI once
        provisioning is completed (SSH is used for provisioning).
    """
    container_name = "%s_keygeneration_container" % name
    packer_script_name = "%s_keygeneration.pkr.hcl" % name

    jar_args = [
        "--key_generation_queue_max_wait_time_seconds",
        "10",
        "--key_generation_queue_message_lease_seconds",
        "120",
        "--param_client",
        "AWS",
        "--decryption_client",
        "ENCLAVE",
    ]

    java_enclave_image(
        name = container_name,
        jar_args = jar_args + additional_jar_args,
        jar_filename = "SplitKeyGenerationApp_deploy.jar",
        jar_target = Label("//java/com/google/scp/coordinator/keymanagement/keygeneration/app/aws:SplitKeyGenerationApp_deploy.jar"),
    )

    generic_enclave_ami_pkr_script(
        name = packer_script_name,
        ami_name = ami_name,
        aws_region = aws_region,
        aws_region_override = aws_region_override,
        # EC2 instance type used to build the AMI.
        ec2_instance = "m5.xlarge",
        subnet_id = subnet_id,
        enable_enclave_debug_mode = debug_mode,
        uninstall_ssh_server = uninstall_ssh_server,
        enclave_allocator = Label("//build_defs/aws/enclave:small.allocator.yaml"),
        enclave_container_image = ":%s.tar" % container_name,
        licenses = _LICENSES_TARGET,
    )

    packer_build(
        name = name,
        packer_file = ":%s" % packer_script_name,
    )
