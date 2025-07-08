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
KEYGEN_IMAGE_NAME=$2
PUBKEYSVC_IMAGE_NAME=$3
ENCKEYSVC_IMAGE_NAME=$4
KEYSTRSVC_IMAGE_NAME=$5
IMAGE_TAG=$6
TAR_BUCKET=$7
TAR_PATH=$8
PBS_IMAGE_NAME=${9:-privacy-budget-service}
FORCE_COORDINATOR_VERSION=${10:-}

BUILD_LOG=$(pwd)/buildlog.txt
cp cc/tools/build/build_container_params.bzl.prebuilt cc/tools/build/build_container_params.bzl
COORDINATOR_VERSION=$(cat version.txt)
if [[ -n ${FORCE_COORDINATOR_VERSION} ]]; then
  COORDINATOR_VERSION="${FORCE_COORDINATOR_VERSION}"
fi

bazel run //coordinator/keygeneration/gcp:key_generation_app_mp_gcp_image_prod \
  --sandbox_writable_path=$HOME/.docker \
  -- -dst "${IMAGE_REPO_PATH}/${KEYGEN_IMAGE_NAME}:${IMAGE_TAG}" \
  | tee "${BUILD_LOG}"

bazel run //coordinator/privacybudget/gcp:pbs_cloud_run_container_image \
   --sandbox_writable_path=$HOME/.docker \
  -- -dst "${IMAGE_REPO_PATH}/${PBS_IMAGE_NAME}:${IMAGE_TAG}" \
  | tee "${BUILD_LOG}"

# Builds and pushes the container image for Public Key Service when
# PUBKEYSVC_IMAGE_NAME is not set to "skip" explicitly.
if [[ ${PUBKEYSVC_IMAGE_NAME} != "skip" ]]; then
  bazel run //coordinator/keyhosting/gcp:public_key_service_image_push \
    --sandbox_writable_path=$HOME/.docker \
    -- -dst "${IMAGE_REPO_PATH}/${PUBKEYSVC_IMAGE_NAME}:${IMAGE_TAG}"
fi

# Builds and pushes the container image for Private Key Service when
# ENCKEYSVC_IMAGE_NAME is not set to "skip" explicitly.
if [[ ${ENCKEYSVC_IMAGE_NAME} != "skip" ]]; then
  bazel run //coordinator/keyhosting/gcp:private_key_service_image_push \
    --sandbox_writable_path=$HOME/.docker \
    -- -dst "${IMAGE_REPO_PATH}/${ENCKEYSVC_IMAGE_NAME}:${IMAGE_TAG}"
fi

# Builds and pushes the container image for Key Storage Service when
# KEYSTRSVC_IMAGE_NAME is not set to "skip" explicitly.
if [[ ${KEYSTRSVC_IMAGE_NAME} != "skip" ]]; then
  bazel run //coordinator/keystorage/gcp:key_storage_service_image_push \
    --sandbox_writable_path=$HOME/.docker \
    -- -dst "${IMAGE_REPO_PATH}/${KEYSTRSVC_IMAGE_NAME}:${IMAGE_TAG}"
fi

bazel build //coordinator/terraform/gcp:multiparty_coordinator_tar

COORDINATOR_TAR_FILE="$(bazel info bazel-bin)/coordinator/terraform/gcp/multiparty_coordinator_tar.tgz"
TAR_MANIPULATION_TMP_DIR=$(mktemp -d -t ci-XXXXXXXXXX)
gunzip < "${COORDINATOR_TAR_FILE}" > "${TAR_MANIPULATION_TMP_DIR}/scp-multiparty-coordinator-${COORDINATOR_VERSION}.tar"

TAR_NAME="ps-gcp-multiparty-coordinator-${COORDINATOR_VERSION}.tgz"
gzip < $TAR_MANIPULATION_TMP_DIR/scp-multiparty-coordinator-${COORDINATOR_VERSION}.tar > ${TAR_NAME}
rm -rf $TAR_MANIPULATION_TMP_DIR && unset TAR_MANIPULATION_TMP_DIR

gsutil cp ${TAR_NAME} gs://${TAR_BUCKET}/${TAR_PATH}/${COORDINATOR_VERSION}/
