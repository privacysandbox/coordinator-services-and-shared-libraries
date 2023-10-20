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

def _sam_config_impl(ctx):
    """
    Generates a new AWS [SAM](https://aws.amazon.com/serverless/sam/) template
    file that replaces {lambda_location} with a relative path to the specified
    jar file. The name of the target matches the name of the generated file.

    SAM by default resolves the CodeURI relative to the location of the template
    file, this rule calculates a relative path between the two files. Note: SAM
    also accept absolute paths but Bazel limits access to the absolute path of
    generated files.
    """
    out = ctx.actions.declare_file(ctx.label.name)

    # calculate the bazel execution root relative to folder of the generated
    # yaml file by replacing all path pieces with ".."
    # E.g.: "bazel-bin/package/foo.yml" => "../.."
    # remove last item [:-1] to drop file component (e.g. "foo.yml").
    relative_root = "/".join([".." for x in out.short_path.split("/")][:-1])

    # Example lambda_location: ../../../../java/com/google/foo_deploy.jar
    lambda_location = "%s/%s" % (relative_root, ctx.file.deploy_jar_file.short_path)

    ctx.actions.expand_template(
        template = ctx.file.sam_config_template,
        output = out,
        substitutions = {
            "{lambda_location}": lambda_location,
        },
    )

    return DefaultInfo(files = depset([out]), runfiles = ctx.runfiles(files = [out, ctx.file.deploy_jar_file]))

sam_config = rule(
    implementation = _sam_config_impl,
    attrs = {
        "deploy_jar_file": attr.label(
            doc = "_deploy.jar file to use as the CodeURI in the sam template",
            allow_single_file = True,
            mandatory = True,
        ),
        "sam_config_template": attr.label(
            doc = "YAML file used to define the sam template",
            allow_single_file = True,
            mandatory = True,
        ),
    },
)
