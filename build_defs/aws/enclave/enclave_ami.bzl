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

def _generic_enclave_ami_pkr_script_impl(ctx):
    enclave_container_image = ctx.file.enclave_container_image

    packer_binary = ctx.executable.packer_binary
    proxy_rpm = ctx.file.proxy_rpm
    enclave_watcher_rpm = ctx.file.enclave_watcher_rpm
    enclave_allocator = ctx.file.enclave_allocator
    packer_template = ctx.file.packer_ami_config
    packer_file = ctx.actions.declare_file(ctx.label.name)
    licenses_tar = ctx.file.licenses

    ctx.actions.expand_template(
        template = packer_template,
        output = packer_file,
        substitutions = {
            "{ami_groups}": ctx.attr.ami_groups,
            "{ami_name}": ctx.attr.ami_name[BuildSettingInfo].value,
            "{aws_region}": ctx.attr.aws_region_override[BuildSettingInfo].value if ctx.attr.aws_region_override != None and ctx.attr.aws_region_override[BuildSettingInfo].value != "None" else ctx.attr.aws_region,
            "{container_filename}": enclave_container_image.basename,
            "{container_path}": enclave_container_image.short_path,
            "{docker_repo}": ctx.attr.enclave_container_image.label.package,
            # Use the input container tag if specified or remove the .tar extension from the container_image name
            "{docker_tag}": ctx.attr.enclave_container_tag if ctx.attr.enclave_container_tag else ctx.attr.enclave_container_image.label.name.replace(".tar", ""),
            "{ec2_instance}": ctx.attr.ec2_instance,
            "{enable_enclave_debug_mode}": "true" if ctx.attr.enable_enclave_debug_mode else "false",
            "{enclave_allocator}": enclave_allocator.short_path,
            "{enclave_watcher_rpm}": ctx.file.enclave_watcher_rpm.short_path,
            "{licenses}": ctx.file.licenses.short_path,
            "{proxy_rpm}": ctx.file.proxy_rpm.short_path,
            "{subnet_id}": ctx.attr.subnet_id,
            "{uninstall_ssh_server}": "true" if ctx.attr.uninstall_ssh_server else "false",
        },
    )

    runfiles = ctx.runfiles(files = [
        packer_file,
        ctx.file.packer_binary,
        ctx.file.proxy_rpm,
        ctx.file.enclave_watcher_rpm,
        ctx.file.enclave_container_image,
        ctx.file.enclave_allocator,
        ctx.file.licenses,
    ])

    return [DefaultInfo(files = depset([packer_file]), runfiles = runfiles)]

generic_enclave_ami_pkr_script = rule(
    implementation = _generic_enclave_ami_pkr_script_impl,
    attrs = {
        "ami_groups": attr.string(
            default = "[]",
        ),
        "ami_name": attr.label(
            mandatory = True,
            providers = [BuildSettingInfo],
        ),
        "aws_region": attr.string(
            mandatory = True,
            default = "us-east-1",
        ),
        "aws_region_override": attr.label(
            mandatory = False,
            providers = [BuildSettingInfo],
        ),
        "ec2_instance": attr.string(
            mandatory = True,
            default = "m5.xlarge",
        ),
        "enable_enclave_debug_mode": attr.bool(
            default = False,
        ),
        "enclave_allocator": attr.label(
            mandatory = True,
            allow_single_file = True,
        ),
        "enclave_container_image": attr.label(
            mandatory = True,
            allow_single_file = True,
        ),
        # Optional. Input the container tag when it is different from the container image name.
        "enclave_container_tag": attr.string(
            default = "",
        ),
        "enclave_watcher_rpm": attr.label(
            default = Label("//build_defs/aws/enclavewatcher:enclave_watcher_rpm"),
            allow_single_file = True,
        ),
        "licenses": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
        "packer_ami_config": attr.label(
            default = Label("//build_defs/aws/enclave:generic_enclave_ami.pkr.hcl"),
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
        "subnet_id": attr.string(
            mandatory = True,
            default = "",
        ),
        "uninstall_ssh_server": attr.bool(
            default = False,
        ),
    },
)
