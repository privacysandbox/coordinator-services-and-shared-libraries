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

BUILD_LOG=$(pwd)/buildlog.txt
cp cc/tools/build/build_container_params.bzl.prebuilt cc/tools/build/build_container_params.bzl
COORDINATOR_VERSION=$(cat version.txt)

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

pushd $TAR_MANIPULATION_TMP_DIR
# Generate the image_params.auto.tfvars file with service container images for mpkhs_primary
mkdir -p environments_mp_primary/shared/mpkhs_primary/ && mkdir -p environments_mp_primary/demo/mpkhs_primary/
cat <<EOT >> environments_mp_primary/shared/mpkhs_primary/image_params.auto.tfvars
########################################################################
# Prefiled values for container image lookup based on released version #
########################################################################

key_generation_image      = "${IMAGE_REPO_PATH}/${KEYGEN_IMAGE_NAME}:${IMAGE_TAG}"
private_key_service_image = "${IMAGE_REPO_PATH}/${ENCKEYSVC_IMAGE_NAME}:${IMAGE_TAG}"
public_key_service_image  = "${IMAGE_REPO_PATH}/${PUBKEYSVC_IMAGE_NAME}:${IMAGE_TAG}"
EOT
ln -s ../../shared/mpkhs_primary/image_params.auto.tfvars environments_mp_primary/demo/mpkhs_primary/image_params.auto.tfvars

# Generate the image_params.auto.tfvars file with service container images for mpkhs_secondary
mkdir -p environments_mp_secondary/shared/mpkhs_secondary/ && mkdir -p environments_mp_secondary/demo/mpkhs_secondary/
cat <<EOT >> environments_mp_secondary/shared/mpkhs_secondary/image_params.auto.tfvars
########################################################################
# Prefiled values for container image lookup based on released version #
########################################################################

key_storage_service_image = "${IMAGE_REPO_PATH}/${KEYSTRSVC_IMAGE_NAME}:${IMAGE_TAG}"
private_key_service_image = "${IMAGE_REPO_PATH}/${ENCKEYSVC_IMAGE_NAME}:${IMAGE_TAG}"
public_key_service_image  = "${IMAGE_REPO_PATH}/${PUBKEYSVC_IMAGE_NAME}:${IMAGE_TAG}"
EOT
ln -s ../../shared/mpkhs_secondary/image_params.auto.tfvars environments_mp_secondary/demo/mpkhs_secondary/image_params.auto.tfvars

# Generate the image_params.auto.tfvars file with service container images for distributedpbs_application primary
mkdir -p environments_mp_primary/shared/distributedpbs_application/ && mkdir -p environments_mp_primary/demo/distributedpbs_application/
cat <<EOT >> environments_mp_primary/shared/distributedpbs_application/image_params.auto.tfvars
########################################################################
# Prefiled values for container image lookup based on released version #
########################################################################

pbs_image_override = "${IMAGE_REPO_PATH}/${PBS_IMAGE_NAME}:${IMAGE_TAG}"
EOT
ln -s ../../shared/distributedpbs_application/image_params.auto.tfvars environments_mp_primary/demo/distributedpbs_application/image_params.auto.tfvars

# Generate the image_params.auto.tfvars file with service container images for distributedpbs_application secondary
mkdir -p environments_mp_secondary/shared/distributedpbs_application/ && mkdir -p environments_mp_secondary/demo/distributedpbs_application/
cat <<EOT >> environments_mp_secondary/shared/distributedpbs_application/image_params.auto.tfvars
########################################################################
# Prefiled values for container image lookup based on released version #
########################################################################

pbs_image_override = "${IMAGE_REPO_PATH}/${PBS_IMAGE_NAME}:${IMAGE_TAG}"
EOT
ln -s ../../shared/distributedpbs_application/image_params.auto.tfvars environments_mp_secondary/demo/distributedpbs_application/image_params.auto.tfvars


tar --append --file=scp-multiparty-coordinator-${COORDINATOR_VERSION}.tar \
  ./environments_mp_primary/shared/mpkhs_primary/image_params.auto.tfvars \
  ./environments_mp_primary/demo/mpkhs_primary/image_params.auto.tfvars \
  ./environments_mp_secondary/shared/mpkhs_secondary/image_params.auto.tfvars \
  ./environments_mp_secondary/demo/mpkhs_secondary/image_params.auto.tfvars \
  ./environments_mp_primary/shared/distributedpbs_application/image_params.auto.tfvars \
  ./environments_mp_primary/demo/distributedpbs_application/image_params.auto.tfvars \
  ./environments_mp_secondary/shared/distributedpbs_application/image_params.auto.tfvars \
  ./environments_mp_secondary/demo/distributedpbs_application/image_params.auto.tfvars
tar --list --file=scp-multiparty-coordinator-${COORDINATOR_VERSION}.tar

popd

TAR_NAME="ps-gcp-multiparty-coordinator-${COORDINATOR_VERSION}.tgz"
gzip < $TAR_MANIPULATION_TMP_DIR/scp-multiparty-coordinator-${COORDINATOR_VERSION}.tar > ${TAR_NAME}
rm -rf $TAR_MANIPULATION_TMP_DIR && unset TAR_MANIPULATION_TMP_DIR

gsutil cp ${TAR_NAME} gs://${TAR_BUCKET}/${TAR_PATH}/$(cat version.txt)/
