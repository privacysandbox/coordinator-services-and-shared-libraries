#!/bin/bash
# Copyright 2024 Google LLC
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

IMAGE_REPO_PATH=$1
IMAGE_NAME=$2
IMAGE_TAG=$3
TAR_BUCKET=$4
TAR_PATH=$5

KEY_GENERATION_LOG=$(pwd)/buildlog.txt
cp cc/tools/build/build_container_params.bzl.prebuilt cc/tools/build/build_container_params.bzl
COORDINATOR_VERSION=$(cat version.txt)

bazel run //coordinator/keygeneration/gcp:key_generation_app_mp_gcp_image_prod \
  --sandbox_writable_path=$HOME/.docker \
  -- -dst "${IMAGE_REPO_PATH}/${IMAGE_NAME}:${IMAGE_TAG}" \
  | tee "${KEY_GENERATION_LOG}"

bazel build //coordinator/terraform/gcp:multiparty_coordinator_tar

COORDINATOR_TAR_FILE="$(bazel info bazel-bin)/coordinator/terraform/gcp/multiparty_coordinator_tar.tgz"
TAR_MANIPULATION_TMP_DIR=$(mktemp -d -t ci-XXXXXXXXXX)
gunzip < "${COORDINATOR_TAR_FILE}" > "${TAR_MANIPULATION_TMP_DIR}/scp-multiparty-coordinator-${COORDINATOR_VERSION}.tar"

pushd $TAR_MANIPULATION_TMP_DIR
mkdir -p environments_mp_primary/shared/mpkhs_primary/ && mkdir -p environments_mp_primary/demo/mpkhs_primary/

cat <<EOT >> environments_mp_primary/shared/mpkhs_primary/image_params.auto.tfvars
########################################################################
# Prefiled values for container image lookup based on released version #
########################################################################

key_generation_image = "${IMAGE_REPO_PATH}/${IMAGE_NAME}:${IMAGE_TAG}"
EOT

ln -s ../../shared/mpkhs_primary/image_params.auto.tfvars environments_mp_primary/demo/mpkhs_primary/image_params.auto.tfvars
tar --append --file=scp-multiparty-coordinator-${COORDINATOR_VERSION}.tar ./environments_mp_primary/shared/mpkhs_primary/image_params.auto.tfvars ./environments_mp_primary/demo/mpkhs_primary/image_params.auto.tfvars
tar --list --file=scp-multiparty-coordinator-${COORDINATOR_VERSION}.tar

popd

TAR_NAME="ps-gcp-multiparty-coordinator-${COORDINATOR_VERSION}.tgz"
gzip < $TAR_MANIPULATION_TMP_DIR/scp-multiparty-coordinator-${COORDINATOR_VERSION}.tar > ${TAR_NAME}
rm -rf $TAR_MANIPULATION_TMP_DIR && unset TAR_MANIPULATION_TMP_DIR

gsutil cp ${TAR_NAME} gs://${TAR_BUCKET}/${TAR_PATH}/$(cat version.txt)/
