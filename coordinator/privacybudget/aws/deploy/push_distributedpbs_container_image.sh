#!/usr/bin/env bash
# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script pushes the PBS container image to a given ECR repository.
#
# Args:
#   required:
#    --aws_region=<value> \ # The aws region for the deployment
#    --ecr_repository_name=<value> \ # The name of the ECR repository
#    --release_version=<value> # The version of this release
#
# `./push_distributedpbs_container_image.sh help` for usage instructions.

set -euo pipefail

function push_pbs_container_image() {
    docker_repository_name=$1
    image_version_tag=$2
    pbs_container_image_tar_path=$3
    aws_region=$4

    default_pbs_image_repo="bazel/cc/pbs/deploy/pbs_server/build_defs"

    # Get the AWS account ID
    aws_account_id="$(aws sts get-caller-identity --query "Account" --output text)"
    # Use the account ID and the region for the registry URL
    container_registry="$aws_account_id.dkr.ecr.$aws_region.amazonaws.com"

    # Validate that repository exists
    aws ecr describe-repositories \
    --region $aws_region \
    --repository-names "$docker_repository_name" 1> /dev/null || \
    (echo "" && \
    echo "ERROR: Repository [$docker_repository_name] does not exist for account $aws_account_id in region $aws_region" && \
    exit 1)

    # Remove all previously loaded pbs container images if any exist
    if [ $(docker images -f "reference=$default_pbs_image_repo" -aq | wc -l) -gt 0 ]; then
        # Let's try to remove it, but not fail if this does not succeed
        docker rmi -f $(docker images -f "reference=$default_pbs_image_repo" -aq) > /dev/null 2> /dev/null
    fi

    # Remove all previously loaded tagged images if any exist
    if [ $(docker images -f "reference=$container_registry" -aq | wc -l) -gt 0 ]; then
        # Let's try to remove it, but not fail if this does not succeed
        docker rmi -f $(docker images -f "reference=$container_registry" -aq) > /dev/null 2> /dev/null
    fi

    # Load the PBS container image
    docker load < "$pbs_container_image_tar_path"

    # Do docker login with the repository password
    aws ecr get-login-password \
    --region $aws_region | docker login \
    --username AWS \
    --password-stdin $container_registry

    # Create new image tag name
    new_image_tag="$container_registry/$docker_repository_name:$image_version_tag"

    # Tag image to push to the registry
    docker tag $default_pbs_image_repo:pbs_container_aws $new_image_tag

    docker push $new_image_tag
}

if [[ "$#" -lt 1 || $1 == "help" ]]; then
help_msg=$(cat <<-END
  \n
  This script will push the PBS container image under ./dist to the given ECR repository.
  Example:
  \n\n
  ./push_distributedpbs_container_image.sh --aws_region=us-east-1 --ecr_repository_name=my-repo --release_version=<UNIQUE-VERSION-TAG>
  \n\n
END
)
  echo -e $help_msg
  exit 1
fi

if [ "$#" -ne 3 ]; then
error_msg=$(cat <<-END
  Must provide all required inputs:\n
    --aws_region=<value>\n
    --ecr_repository_name=<value>\n
    --release_version=<value>\n
END
)
  echo -e $error_msg
  exit 1
fi

auto_approve=false

while [ $# -gt 0 ]; do
  case "$1" in
    --aws_region=*)
      aws_region="${1#*=}"
      ;;
    --ecr_repository_name=*)
      ecr_repository_name="${1#*=}"
      ;;
    --release_version=*)
      release_version="${1#*=}"
      ;;
    *)
      printf "***************************\n"
      printf "* Error: Invalid argument.*\n"
      printf "***************************\n"
      exit 1
  esac
  shift
done

container_image_tar="./dist/reproducible_pbs_container_aws.tar"
if [ ! -f "$container_image_tar" ]; then
    echo "ERROR: PBS container image tar [$container_image_tar] does not exist."
    exit 1
fi

push_pbs_container_image "$ecr_repository_name" "$release_version" "$container_image_tar" "$aws_region"

echo ""
echo "Pushed container image with tag: $release_version"
echo ""
