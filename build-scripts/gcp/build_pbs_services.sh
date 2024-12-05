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

# This script is a wrapper for building and uploading the privacy budget service
# to the container registry

set -eux -o pipefail

BUILD_LOG="${PWD}/buildlog.txt"
COMPONENT="$1"
IMAGE_REPO_PATH="$2"
IMAGE_NAME="$3"
IMAGE_TAG="$4"

build_pbs_service() {
  bazel run //coordinator/privacybudget/gcp:pbs_container_image \
    --sandbox_writable_path="$HOME/.docker" \
    -- -dst "$1"
}

build_pbs_cloud_run_service() {
  bazel run //coordinator/privacybudget/gcp:pbs_cloud_run_container_image \
    --sandbox_writable_path="$HOME/.docker" \
    -- -dst "$1"
}

main() {
  cp cc/tools/build/build_container_params.bzl.prebuilt cc/tools/build/build_container_params.bzl
  local destination
  destination="${IMAGE_REPO_PATH}/${IMAGE_NAME}:${IMAGE_TAG}"

  case "${COMPONENT}" in
    pbs)
      build_pbs_service "${destination}" | tee "${BUILD_LOG}"
      ;;
    pbs_cloud_run)
      build_pbs_cloud_run_service "${destination}" | tee "${BUILD_LOG}"
      ;;
    *)
      echo "Unknown PBS component"
      exit 1
      ;;
  esac
}

main "$@"
