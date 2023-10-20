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

"""Bazel rules for interacting with Packer (https://packer.io)."""

_BUILD_SCRIPT_TEMPLATE = """#!/bin/bash
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

def _packer_build_impl(ctx):
    packer_binary = ctx.executable.packer_binary
    packer_file = ctx.file.packer_file
    if not packer_file.basename.endswith(".pkr.hcl"):
        fail("Input packer_file must have file extension '.pkr.hcl'")

    script = ctx.actions.declare_file("%s_script.sh" % ctx.label.name)
    script_content = _BUILD_SCRIPT_TEMPLATE.format(
        packer = packer_binary.short_path,
        packer_file = packer_file.short_path,
    )

    ctx.actions.write(script, script_content, is_executable = True)

    # Associate runfiles of input packer file as runfiles of this execution in
    # order to make them available when packer is running.
    additional_runfiles = ctx.attr.packer_file[DefaultInfo].default_runfiles

    return [DefaultInfo(
        executable = script,
        files = depset([script]),
        runfiles = ctx.runfiles(files = [packer_binary, packer_file]).merge(additional_runfiles),
    )]

packer_build = rule(
    doc = """
    Wrapper for an execution of `packer build`, executes packer build inside of
    the bazel execution environment.

    Expects that all dependencies for the input packer_file are properly
    associated as bazel "runfiles" of the target's DefaultInfo provider.
    """,
    implementation = _packer_build_impl,
    attrs = {
        "packer_binary": attr.label(
            default = Label("@packer//:packer"),
            executable = True,
            cfg = "exec",
            allow_single_file = True,
        ),
        "packer_file": attr.label(
            mandatory = True,
            allow_single_file = True,
            # TODO: should ensuring runfiles are passed forward be enforced by
            # requiring a custom "PackerInfo" provider?
            doc = "Packer file to run. Must end in '.pkr.hcl'",
        ),
    },
    executable = True,
)
