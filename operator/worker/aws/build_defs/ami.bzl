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

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")

# Executable rule for running packer to build an AWS worker AMI image
def _packer_worker_ami_impl(ctx):
    enclave_tar = ctx.file.enclave_container_image
    packer_binary = ctx.executable.packer_binary
    proxy_rpm = ctx.file.proxy_rpm
    worker_watcher_rpm = ctx.file.worker_watcher_rpm
    startup_script = ctx.file.startup_script
    enclave_allocator = ctx.file.enclave_allocator
    packer_template = ctx.file.packer_ami_config
    packer_file = ctx.actions.declare_file("%s_packer.pkr.hcl" % ctx.label.name)
    licenses_tar = ctx.file.licenses
    user_rpms = ctx.files.user_rpms

    ec2_provision_script = ctx.actions.declare_file(
        "%s_ec2_startup.sh" % ctx.label.name,
    )

    # TODO: throw an error if the label name for the enclave container image
    # does not end in .tar

    ctx.actions.expand_template(
        template = startup_script,
        output = ec2_provision_script,
        substitutions = {
            "{container_file}": ctx.attr.enclave_container_image.label.name,
            "{docker_repo}": ctx.attr.enclave_container_image.label.package,
            # Remove the .tar extension for the docker tag
            "{docker_tag}": ctx.attr.enclave_container_image.label.name.replace(".tar", ""),
        },
    )

    allocator_file_expanded = ctx.actions.declare_file(
        "%s_allocator.yaml" % ctx.label.name,
    )

    ctx.actions.expand_template(
        template = enclave_allocator,
        output = allocator_file_expanded,
        substitutions = {
            "{cpus}": str(ctx.attr.enclave_cpus),
            "{memory_mib}": str(ctx.attr.enclave_memory_mib),
        },
    )

    rpm_list = [proxy_rpm.short_path, worker_watcher_rpm.short_path]
    rpm_list.extend([
        rpm_file.short_path
        for rpm_file in user_rpms
    ])

    ctx.actions.expand_template(
        template = packer_template,
        output = packer_file,
        substitutions = {
            "{ami_groups}": ctx.attr.ami_groups,
            "{ami_name}": ctx.attr.ami_name[BuildSettingInfo].value,
            "{aws_region}": ctx.attr.aws_region[BuildSettingInfo].value,
            "{container_filename}": enclave_tar.basename,
            "{container_path}": enclave_tar.short_path,
            "{ec2_instance}": ctx.attr.ec2_instance,
            "{enable_worker_debug_mode}": "true" if ctx.attr.enable_worker_debug_mode else "false",
            "{enclave_allocator}": allocator_file_expanded.short_path,
            "{licenses}": licenses_tar.short_path,
            "{provision_script}": ec2_provision_script.short_path,
            "{rpms}": '["' + '","'.join(rpm_list) + '"]',
            "{subnet_id}": ctx.attr.subnet_id[BuildSettingInfo].value,
            "{uninstall_ssh_server}": "true" if ctx.attr.uninstall_ssh_server else "false",
        },
    )

    script_template = """#!/bin/bash
set -e
echo "Preparing to run Packer ({packer})"
echo "Packer config:"
echo "-------------------------"
cat {packer_file}
echo "-------------------------"

{packer} version
{packer} init {packer_file}
{packer} build {packer_file}
"""

    script = ctx.actions.declare_file("%s_script.sh" % ctx.label.name)
    script_content = script_template.format(
        packer = packer_binary.short_path,
        packer_file = packer_file.short_path,
    )
    ctx.actions.write(script, script_content, is_executable = True)

    runfiles = ctx.runfiles(files = [
        packer_binary,
        packer_file,
        proxy_rpm,
        worker_watcher_rpm,
        enclave_tar,
        ec2_provision_script,
        allocator_file_expanded,
        licenses_tar,
    ] + user_rpms)
    return [DefaultInfo(
        executable = script,
        files = depset([script, packer_file, ec2_provision_script, allocator_file_expanded]),
        runfiles = runfiles,
    )]

packer_worker_ami = rule(
    implementation = _packer_worker_ami_impl,
    attrs = {
        "ami_groups": attr.string(
            default = "[]",
        ),
        "ami_name": attr.label(
            mandatory = True,
            providers = [BuildSettingInfo],
        ),
        "aws_region": attr.label(
            mandatory = True,
            providers = [BuildSettingInfo],
        ),
        "ec2_instance": attr.string(
            mandatory = True,
            default = "m5.xlarge",
        ),
        "enable_worker_debug_mode": attr.bool(
            default = False,
        ),
        "enclave_allocator": attr.label(
            default = Label("//build_defs/aws/enclave:allocator.template.yaml"),
            allow_single_file = True,
        ),
        "enclave_container_image": attr.label(
            mandatory = True,
            allow_single_file = True,
        ),
        "enclave_cpus": attr.int(
            default = 2,
        ),
        "enclave_memory_mib": attr.int(
            default = 7168,
        ),
        "licenses": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
        "packer_ami_config": attr.label(
            default = Label("//operator/worker/aws:aggregation_worker_ami.pkr.hcl"),
            allow_single_file = True,
        ),
        "packer_binary": attr.label(
            default = Label("@packer//:packer"),
            executable = True,
            cfg = "exec",
            allow_single_file = True,
        ),
        "proxy_rpm": attr.label(
            default = Label("//cc/aws/proxy:vsockproxy_rpm"),
            cfg = "exec",
            allow_single_file = True,
        ),
        "startup_script": attr.label(
            default = Label("//operator/worker/aws:setup_enclave.sh"),
            allow_single_file = True,
        ),
        "subnet_id": attr.label(
            mandatory = True,
            providers = [BuildSettingInfo],
        ),
        "uninstall_ssh_server": attr.bool(
            default = False,
        ),
        "user_rpms": attr.label_list(
            allow_empty = True,
            default = [],
        ),
        "worker_watcher_rpm": attr.label(
            default = Label("//operator/worker/aws/enclave:aggregate_worker_rpm"),
            allow_single_file = True,
        ),
    },
    executable = True,
)
