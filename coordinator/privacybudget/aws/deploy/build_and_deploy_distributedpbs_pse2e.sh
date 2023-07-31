#!/usr/bin/env bash
# Copyright 2023 Google LLC
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


# This script is intended to be run as part of kokoro postsubmit e2e script to deploy the latest code
# to PBS pse2e environments (both primary and secondary).
#
# This script builds current PBS code as a .tar, goes to both pse2e primary and secondary environment folders
# and executes deploy_distributedpbs.sh on them to deploy the built PBS code.
#
# This script depends on push_distributedpbs_container_image.sh and builds .tar at the location
# `coordinator/terraform/aws/dist/`
#
set -euo pipefail

pwdir=`pwd`

# kokoro's '/' mount point starts at scp/

# Change directory to the environments directory where distributed_mp_primary and distributed_mp_secondary are present.
cd coordinator/terraform/aws/
# deploy_distributedpbs.sh script depends on functions defined in push_distributedpbs_container_image.sh and expects it to be present in pwd.
cp ../../privacybudget/aws/deploy/push_distributedpbs_container_image.sh .
bazel build //cc/pbs/deploy/pbs_server/build_defs:reproducible_pbs_container_aws
# Create a directory to copy over the generated .tar file of built PBS code. deploy_distributedpbs script assumes dist/ to be present.
mkdir -p dist
rm -f dist/*.tar
cp $(bazel info bazel-bin)/cc/pbs/deploy/pbs_server/build_defs/reproducible_pbs_container_aws.tar dist/
$(bazel info workspace)/coordinator/privacybudget/aws/deploy/deploy_distributedpbs.sh --aws_region=us-east-1 --environment=pse2e --coordinator=secondary --release_version=pse2e --auto_approve=true
$(bazel info workspace)/coordinator/privacybudget/aws/deploy/deploy_distributedpbs.sh --aws_region=us-east-1 --environment=pse2e --coordinator=primary --release_version=pse2e --auto_approve=true
rm push_distributedpbs_container_image.sh
rm -f dist/*.tar

cd $pwdir
echo "Done :)"