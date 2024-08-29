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

# This script pushes the PBS container image to a given artifact registry.
#
# Args:
#   required:
#    --gcp_region=<value> \ # The GCP region for the deployment
#    --artifact_registry_name=<value> \ # The name of the artifact registry repository
#    --release_version=<value> \ # The version of this release
#    --gcp_project_id=<value> \ # The project ID for this deployment
#
# `./push_distributedpbs_container_image.sh help` for usage instructions.

set -eux
set -o pipefail

function push_pbs_container_image() {
    local default_pbs_image_repo
    local container_registry
    default_pbs_image_repo="bazel/cc/pbs/deploy/pbs_server/build_defs"
    container_registry="$gcp_region-docker.pkg.dev"

    # Validate that repository exists
    gcloud artifacts repositories describe "$artifact_registry_name" --project "$gcp_project_id" --location "$gcp_region" || \
    (echo "" && \
    echo "ERROR: Repository [$artifact_registry_name] does not exist for project $gcp_project_id in region $gcp_region" && \
    exit 1)

    # Remove all previously loaded pbs container images if any exist
    if [ $(docker images -f "reference=$default_pbs_image_repo" -aq | wc -l) -gt 0 ]; then
        # Let's try to remove it, but not fail if this does not succeed
        docker rmi -f $(docker images -f "reference=$default_pbs_image_repo" -aq) || true
    fi

    # Remove all previously loaded tagged images if any exist
    if [ $(docker images -f "reference=$container_registry" -aq | wc -l) -gt 0 ]; then
        # Let's try to remove it, but not fail if this does not succeed
        docker rmi -f $(docker images -f "reference=$container_registry" -aq) || true
    fi

    # Load the PBS container image
    docker load < "$pbs_container_image_tar_path"

    # Setup the docker repository credentials helper
    gcloud --quiet auth configure-docker $container_registry

    # Create new image tag name
    local new_image_tag
    new_image_tag="$container_registry/$gcp_project_id/$artifact_registry_name/pbs-image:$release_version"

    # Tag image to push to the registry
    docker tag $default_pbs_image_repo:pbs_container_gcp $new_image_tag
    docker push $new_image_tag
}

if [[ "$#" -lt 1 || $1 == "help" ]]; then
help_msg=$(cat <<-END
  \n
  This script will push the PBS container image under ./dist to the given artifact registry repository.
  Example:
  \n\n
  ./push_distributedpbs_container_image.sh --gcp_region=us-central1 --artifact_registry_name=my-repo --release_version=<UNIQUE-VERSION-TAG> --gcp_project_id=my-project
  \n\n
END
)
  echo -e $help_msg
  exit 1
fi

if [ "$#" -ne 4 ]; then
error_msg=$(cat <<-END
  Must provide all required inputs:\n
    --gcp_region=<value>\n
    --artifact_registry_name=<value>\n
    --release_version=<value>\n
    --gcp_project_id=<value>\n
END
)
  echo -e $error_msg
  exit 1
fi

auto_approve=false

while [ $# -gt 0 ]; do
  case "$1" in
    --gcp_region=*)
      gcp_region="${1#*=}"
      ;;
    --artifact_registry_name=*)
      artifact_registry_name="${1#*=}"
      ;;
    --release_version=*)
      release_version="${1#*=}"
      ;;
    --gcp_project_id=*)
      gcp_project_id="${1#*=}"
      ;;
    *)
      printf "***************************\n"
      printf "* Error: Invalid argument.*\n"
      printf "***************************\n"
      exit 1
  esac
  shift
done

pbs_container_image_tar_path="./dist/reproducible_pbs_container_gcp.tar"
if [ ! -f "$pbs_container_image_tar_path" ]; then
    echo "ERROR: PBS container image tar [$pbs_container_image_tar_path] does not exist."
    exit 1
fi

push_pbs_container_image

echo ""
echo "Pushed container image with tag: $release_version"
echo ""
