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
load("//build_defs/cc/aws/enclave:container.bzl", "cc_enclave_image")
load("//build_defs/shared:packer_build.bzl", "packer_build")

_LICENSES_TARGET = Label("//licenses:licenses_tar")

def cpio_test_ami(
        name,
        binary_filename,
        binary_target,
        ami_name = Label("//cc/public/cpio/examples/deploy/aws/enclave:cpio_test_ami_name"),
        aws_region = "us-east-1",
        subnet_id = "",
        debug_mode = True):
    """
    Creates a runnable target for deploying a CPIO test AMI.

    To deploy an AMI, `bazel run` the provided name of this target.
    The generated Packer script is available as `$name.pkr.hcl`
    """
    container_name = "%s_container" % name
    cc_enclave_image(
        name = container_name,
        binary_filename = binary_filename,
        binary_target = Label(binary_target),
        pkgs_to_install = [
            "ca-certificates",
            "curl",
            "rsyslog",
            "libatomic1",
            "libxml2-dev",
            "netbase",
        ],
    )

    reproducible_container_name = "%s_reproducible_container" % name

    # This rule can be used to build the container image in a reproducible manner.
    # It builds the image within a container with fixed libraries and dependencies.
    native.genrule(
        name = reproducible_container_name,
        srcs = [
            Label("//cc/public/tools:build_reproducible_container_image.sh"),
            Label("//:source_code_tar"),
            Label("//cc/tools/build:prebuilt_cc_build_container_image.tar"),
        ],
        outs = ["%s.tar" % reproducible_container_name],
        # NOTE: This order matters
        # Arguments:
        # $1 is the output tar, that is, the path where this rule generates its output ($@)
        # $2 is the packaged SCP source code ($(location //:source_code_tar))
        # $3 is the build container image TAR path
        # $4 is the name of the container to be built
        # $5 is the build container target path
        # $6+ are the build args
        cmd = "./$(location //cc/public/tools:build_reproducible_container_image.sh) $@ $(location //:source_code_tar) $(location //cc/tools/build:prebuilt_cc_build_container_image.tar) %s %s %s %s" % (container_name, "//cc/public/cpio/examples/deploy/aws/enclave:%s.tar" % container_name, "--//cc/public/cpio/interface:run_inside_tee=True", "--//cc/public/cpio/interface:platform=aws"),
        tags = ["manual"],
    )

    packer_script_name = "%s.pkr.hcl" % name
    generic_enclave_ami_pkr_script(
        name = packer_script_name,
        ami_name = ami_name,
        aws_region = aws_region,
        # EC2 instance type used to build the AMI.
        ec2_instance = "m5.xlarge",
        subnet_id = subnet_id,
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
