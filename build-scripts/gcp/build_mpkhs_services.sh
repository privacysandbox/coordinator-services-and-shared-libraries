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

# This script is wrapper for building the multiparty key hosting service
# components seperately

set -eux -o pipefail

BUILD_LOG="${PWD}/buildlog.txt"
COMPONENT="$1"
IMAGE_REPO_PATH="$2"
IMAGE_NAME="$3"
IMAGE_TAG="$4"

build_key_generation_service() {
    bazel run //coordinator/keygeneration/gcp:key_generation_app_mp_gcp_image_prod \
    --sandbox_writable_path="${HOME}/.docker" \
    -- -dst "$1"
}

build_public_key_service() {
  bazel run //coordinator/keyhosting/gcp:public_key_service_image_push \
    --sandbox_writable_path="${HOME}/.docker" \
    -- -dst "$1"
}

build_private_key_service() {
  bazel run //coordinator/keyhosting/gcp:private_key_service_image_push \
    --sandbox_writable_path="${HOME}/.docker" \
    -- -dst "$1"
}

build_key_storage_service() {
  bazel run //coordinator/keystorage/gcp:key_storage_service_image_push \
    --sandbox_writable_path="${HOME}/.docker" \
    -- -dst "$1"
}

main() {
  cp cc/tools/build/build_container_params.bzl.prebuilt cc/tools/build/build_container_params.bzl
  local destination
  destination="${IMAGE_REPO_PATH}/${IMAGE_NAME}:${IMAGE_TAG}"

  case "${COMPONENT}" in
    key_generation)
      build_key_generation_service "${destination}" | tee "${BUILD_LOG}"
      ;;
    key_storage)
      build_key_storage_service "${destination}" | tee "${BUILD_LOG}"
      ;;
    private_key)
      build_private_key_service "${destination}" | tee "${BUILD_LOG}"
      ;;
    public_key)
      build_public_key_service "${destination}" | tee "${BUILD_LOG}"
      ;;
    *)
      echo "Unknown MPKHS component"
      exit 1
      ;;
  esac
}

main "$@"
