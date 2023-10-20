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

def _generate_avro_impl(ctx):
    out = ctx.actions.declare_file("%s.avro" % ctx.label.name)

    args = ctx.actions.args()
    args.add("--browser_report_file_path", out)
    args.add("--num_reports", ctx.attr.num_reports)
    args.add("--distribution", "FILE")
    args.add("--distribution_file_path", ctx.file.human_readable_reports)
    args.add("--asymmetric_key_file_path", ctx.file.key)

    ctx.actions.run(
        executable = ctx.executable._simulation,
        arguments = [args],
        inputs = [ctx.file.human_readable_reports, ctx.file.key],
        outputs = [out],
    )

    return [DefaultInfo(
        files = depset([out]),
        runfiles = ctx.runfiles(files = [out]),
    )]

generate_avro = rule(
    implementation = _generate_avro_impl,
    attrs = {
        "human_readable_reports": attr.label(
            doc = "Text file containing human-readable reports.",
            allow_single_file = True,
            mandatory = True,
        ),
        "key": attr.label(
            doc = "Key file for encryption the reports",
            allow_single_file = True,
            mandatory = True,
        ),
        # TODO: remove the attr below, the tool should figure out on its own how
        # many reports are there.
        "num_reports": attr.int(
            doc = "Number of reports that should be generated.",
            mandatory = True,
        ),
        "_simulation": attr.label(
            default = Label("//java/com/google/scp/simulation:SimluationRunner"),
            executable = True,
            cfg = "target",
        ),
    },
)
