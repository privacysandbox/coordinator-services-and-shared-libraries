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

def _gcs_package_release_impl(ctx):
    package_file = ctx.file.package_target
    gcloud_sdk = ctx.file.gcloud_sdk

    script_template = """#!/bin/bash
set -eux
# parse arguments and set up variables
artifact_base_name="{artifact_base_name}"
version=""
for ARGUMENT in "$@"
do
    case $ARGUMENT in
        --version=*)
        version=$(echo $ARGUMENT | cut -f2 -d=)
        ;;
        *)
        printf "ERROR: invalid argument $ARGUMENT\n"
        exit 1
        ;;
    esac
done
## check variables and arguments
if [[ "$version" == "" ]]; then
    printf "ERROR: --version argument is not provided\n"
    exit 1
fi
if [[ "$artifact_base_name" != *"{{VERSION}}"* ]]; then
    printf "ERROR: artifact_base_name must include {{VERSION}} substring\n"
    exit 1
fi
artifact_name=$(echo $artifact_base_name | sed -e "s/{{VERSION}}/$version/g")
# upload artifact to gcs
echo "Preparing release package to gcs, with {gcloud_sdk}/bin/gsutil"
{gcloud_sdk}/bin/gsutil --version
echo "bazel working directory:"
pwd
echo "-------------------------"
outputfile=$(mktemp)
{gcloud_sdk}/bin/gsutil -D cp -n {package_file} gs://{release_bucket}/{release_key}/$version/$artifact_name 2>&1 | tee $outputfile
if grep -q "Skipping" "$outputfile"; then
  echo "ERROR: Artifact already exists"
  exit 1
fi
"""

    script = ctx.actions.declare_file("%s_script.sh" % ctx.label.name)
    script_content = script_template.format(
        gcloud_sdk = gcloud_sdk.short_path,
        package_file = package_file.short_path,
        release_bucket = ctx.attr.release_bucket,
        release_key = ctx.attr.release_key,
        artifact_base_name = ctx.attr.artifact_base_name,
    )
    ctx.actions.write(script, script_content, is_executable = True)

    runfiles = ctx.runfiles(files = [
        gcloud_sdk,
        package_file,
    ])

    return [DefaultInfo(
        executable = script,
        files = depset([script, package_file]),
        runfiles = runfiles,
    )]

gcs_package_release_rule = rule(
    implementation = _gcs_package_release_impl,
    attrs = {
        "artifact_base_name": attr.string(
            mandatory = True,
        ),
        "gcloud_sdk": attr.label(
            allow_single_file = True,
            default = "@google-cloud-sdk//:google-cloud-sdk",
        ),
        "package_target": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
        "release_bucket": attr.string(
            mandatory = True,
        ),
        "release_key": attr.string(
            mandatory = True,
        ),
    },
    executable = True,
)

def gcs_package_release(
        *,
        name,
        package_target,
        release_bucket,
        release_key,
        artifact_base_name,
        gcloud_sdk = "@google-cloud-sdk//:google-cloud-sdk"):
    """
    Creates targets for releasing coordinator artifact to gcs.

    This rule only accepts releasing one artifact (e.g. a tar file). The path in which the artifact is stored
    is specified by the gcs bucket name, key, artifact base name, and version. The naming convention is as below:
    `gs://bucket/key/VERSION/name_of_artifact_VERSION.tgz`

    targets:
      1. '%s_script.sh': script for running gsutil to copy code package to gcs.
      2. artifact file: the package file being uploaded to gcs. This is built from the
        specified build target (e.g. a `pkg_tar` target).

    Args:
        name: The target name used to generate the targets described above.
        package_target: Path to the package build target.
        release_bucket: Gcs bucket to which the release the artifact.
        release_key: Gcs key for the release artifact.
        artifact_base_name: base name of the artifact. The base name should include a "{VERSION}" substring which will be
            replaced with the version argument. e.g. "scp-singleparty-trustedparty-{VERSION}.tgz".
        gcloud_sdk: Path to google cloud sdk for linux.
    """

    gcs_package_release_rule(
        name = name,
        gcloud_sdk = gcloud_sdk,
        package_target = package_target,
        release_bucket = release_bucket,
        release_key = release_key,
        artifact_base_name = artifact_base_name,
    )
