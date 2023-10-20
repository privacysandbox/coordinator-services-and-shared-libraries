#!/bin/bash
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

set -eux
set -o pipefail

cp cc/tools/build/build_container_params.bzl.prebuilt cc/tools/build/build_container_params.bzl
KEY_GENERATION_LOG=$(pwd)/buildlog.txt
COORDINATOR_VERSION=$(cat version.txt)
KEY_GENERATION_AMI="aggregation-service-keygeneration-enclave-prod-${COORDINATOR_VERSION}"

bazel run //coordinator/keygeneration/aws:prod_keygen_ami \
	--sandbox_writable_path=$HOME/.docker \
	--//coordinator/keygeneration/aws:keygeneration_ami_name="${KEY_GENERATION_AMI}" \
	--//coordinator/keygeneration/aws:keygeneration_ami_region="${AWS_DEFAULT_REGION}" | tee "${KEY_GENERATION_LOG}"

grep -o '"PCR0": "[^"]*"' "${KEY_GENERATION_LOG}" | awk -F'"' '{print $4}'

COORDINATOR_TAR_FILE=$(bazel info bazel-bin)/coordinator/terraform/aws/multiparty_coordinator_tar.tgz
KEY_GENERATION_AMI="aggregation-service-keygeneration-enclave-prod-${COORDINATOR_VERSION}"
TAR_MANIPULATION_TMP_DIR=$(mktemp -d -t ci-XXXXXXXXXX)

bazel build //coordinator/terraform/aws:multiparty_coordinator_tar

gunzip < $COORDINATOR_TAR_FILE > $TAR_MANIPULATION_TMP_DIR/scp-multiparty-coordinator-${COORDINATOR_VERSION}.tar
pushd $TAR_MANIPULATION_TMP_DIR
mkdir -p environments_mp_primary/shared/mpkhs_primary/ && mkdir -p environments_mp_primary/demo/mpkhs_primary/

cat <<EOT >> environments_mp_primary/shared/mpkhs_primary/ami_params.auto.tfvars
##########################################################
# Prefiled values for AMI lookup based on relase version #
##########################################################

key_generation_ami_name_prefix = "${KEY_GENERATION_AMI}"
key_generation_ami_owners      = ["${AWS_ACCOUNT_ID}"]
EOT

ln -s ../../shared/mpkhs_primary/ami_params.auto.tfvars environments_mp_primary/demo/mpkhs_primary/ami_params.auto.tfvars
tar --append --file=scp-multiparty-coordinator-${COORDINATOR_VERSION}.tar ./environments_mp_primary/shared/mpkhs_primary/ami_params.auto.tfvars ./environments_mp_primary/demo/mpkhs_primary/ami_params.auto.tfvars
tar --list --file=scp-multiparty-coordinator-${COORDINATOR_VERSION}.tar
popd
gzip < $TAR_MANIPULATION_TMP_DIR/scp-multiparty-coordinator-${COORDINATOR_VERSION}.tar > multiparty-coordinator-${COORDINATOR_VERSION}.tgz
rm -rf $TAR_MANIPULATION_TMP_DIR
unset TAR_MANIPULATION_TMP_DIR
