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

"""Rules for generating "allocator.yaml" files needed for nitro enclaves."""

def _allocator_yaml_impl(ctx):
    enclave_allocator_template = ctx.file._enclave_allocator_template
    if not ctx.label.name.endswith(".yaml"):
        fail("allocator_yaml name attr must have file extension '.yaml'")

    generated_allocator = ctx.actions.declare_file(ctx.label.name)

    ctx.actions.expand_template(
        template = enclave_allocator_template,
        output = generated_allocator,
        substitutions = {
            "{cpus}": str(ctx.attr.enclave_cpus),
            "{memory_mib}": str(ctx.attr.enclave_memory_mib),
        },
    )

    return [DefaultInfo(files = depset([generated_allocator]))]

allocator_yaml = rule(
    doc = """
    Generates a allocator.yaml file with the specified cpu and memory requirements
    """,
    implementation = _allocator_yaml_impl,
    attrs = {
        "enclave_cpus": attr.int(
            default = 2,
            mandatory = True,
        ),
        "enclave_memory_mib": attr.int(
            default = 7168,
            mandatory = True,
        ),
        "_enclave_allocator_template": attr.label(
            default = Label("//build_defs/aws/enclave:allocator.template.yaml"),
            allow_single_file = True,
        ),
    },
)
