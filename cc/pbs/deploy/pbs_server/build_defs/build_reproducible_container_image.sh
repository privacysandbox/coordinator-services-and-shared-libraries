#!/bin/bash
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


# This script is intended to be called from bazel rule reproducible_pbs_container_*
# It builds the PBS container image within a container to guarantee a reproducible build.
# Arguments:
# $1 is the output tar, that is, the path where this rule generates its output
# $2 is the packaged SCP source code
# $3 is the PBS build type
# $4 is the build container image tag

set -euo pipefail

output_tar=$1
source_code_tar=$2
build_type=$3
build_container_tar_path=$4

build_container_image_name="bazel/cc/tools/build:prebuilt_cc_build_container_image"
docker load < $build_container_tar_path

# Validate build type
if [ "$build_type" == "aws_integration_test" ]; then
    echo "Building PBS container for AWS integration test"
elif [ "$build_type" == "gcp_integration_test" ]; then
    echo "Building PBS container for GCP integration test"
elif [ "$build_type" == "aws" ]; then
    echo "Building PBS container for AWS"
elif [ "$build_type" == "gcp" ]; then
    echo "Building PBS container for GCP"
else
    echo "Invalid build type specified. Exiting."
    exit 1
fi

pbs_container_name="pbs_container_$build_type.tar"

pbs_container_bazel_tar_path="//cc/pbs/deploy/pbs_server/build_defs:$pbs_container_name"
timestamp=$(date "+%Y%m%d-%H%M%S%N")
build_container_name="pbs_reproducible_build_$timestamp"

run_on_exit() {
    echo ""
    if [ "$1" == "0" ]; then
        echo "Done :)"
    else
        echo "Done :("
    fi
    
    docker rm -f $build_container_name > /dev/null 2> /dev/null
}

# Make sure run_on_exit runs even when we encounter errors
trap "run_on_exit 1" ERR

# Set the output directory for the container build
docker_bazel_output_dir=/tmp/pbs_reproducible_build/$build_container_name

docker -D run -d -i \
--privileged \
-v /var/run/docker.sock:/var/run/docker.sock \
-v $docker_bazel_output_dir:/tmp/bazel_build_output \
--name $build_container_name \
$build_container_image_name

# Copy the scp source code into the build container
# The -L is important as we are copying from a symlink
docker cp -L $source_code_tar $build_container_name:/

# Extract the source code
docker exec $build_container_name tar -xf /source_code_tar.tar

# Set the build output directory
docker exec $build_container_name \
bash -c "echo 'startup --output_user_root=/tmp/bazel_build_output' >> /scp/.bazelrc"

docker exec -w /scp $build_container_name \
bash -c "bazel build -c opt --action_env=BAZEL_CXXOPTS=\"-std=c++17\" --//cc/pbs/pbs_server/src/pbs_instance:platform=\"$build_type\" $pbs_container_bazel_tar_path"

# Change the build output directory permissions to the user running this script
user_id="$(id -u)"
docker exec $build_container_name chown -R $user_id:$user_id /tmp/bazel_build_output

# Copy the container image to the output
cp $(find $docker_bazel_output_dir -name $pbs_container_name) $output_tar

run_on_exit 0
