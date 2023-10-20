#!/usr/bin/env bash
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

#
# Release operator artifacts.
# Builds and releases the following:
#     - Aggregate worker AMI
#           Builds and registers aggregate worker AMI (note that proper credentials should be set up)
#           uses the target `//operator/worker/aws:scp_worker_aws`
#     - Operator terraform applications and FE service
#           Builds a tar file of Operator terraform applications and FE service, and puts it on the specified gcs bucket
#           uses the target `//operator/aws:operator_aws_release`
#
# Arguments:
#   --version: release version string, e.g. "1.0.0"
#   --base_ami_name: base name for the AMI (version and build time will be appended to construct full name)
set -eux
version=""
base_ami_name=""
for ARGUMENT in "$@"
do
    case $ARGUMENT in
        --version=*)
        version=$(echo $ARGUMENT | cut -f2 -d=)
        ;;
        --base_ami_name=*)
        base_ami_name=$(echo $ARGUMENT | cut -f2 -d=)
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
if [[ "$base_ami_name" == "" ]]; then
    printf "ERROR: --base_ami_name argument is not provided\n"
    exit 1
fi

AMI_NAME="$base_ami_name""_""$version"
AMI_OWNERS="[$(aws sts get-caller-identity --query 'Account')]"

# specify gcc version
if [[ -x /usr/bin/g++-9 ]]; then
    export CXX=/usr/bin/g++-9
    export CC=/usr/bin/gcc-9
else
  printf "ERROR: Installing g++-9 is required\n"
  exit 1
fi

# build and release artifacts
bazel run //operator/worker/aws:scp_worker_aws --//operator/worker/aws:ami_name_flag=$AMI_NAME
bazel run //operator/aws:operator_aws_release \
  --//operator/terraform/aws/applications/operator-service:ami_name_flag=$AMI_NAME \
  --//operator/terraform/aws/applications/operator-service:ami_owners_flag=$AMI_OWNERS \
  -- --version=$version
