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

# This script builds the container image for SDK and PBS within a container to
# guarantee a reproducible build.

set -euo pipefail

output_tar=$1
source_code_tar=$2
build_container_tar_path=$3
container_name_to_build=$4
container_tar_target_path=$5

# build_flags. 'opt' for optimized. 'dbg' for debug.
# go/cpp-stacktraces#s3
build_flags="-c opt --copt=-gmlt --strip=never"

args=""

# Customized flags/args start from the 6th argument.
for ((i=6; i<=$#; i++))
do
  args+=${!i}" "
done

build_container_image_name="bazel/cc/tools/build:prebuilt_cc_build_container_image"
docker load < $build_container_tar_path

timestamp=$(date "+%Y%m%d-%H%M%S%N")
container_name="reproducible_build_$timestamp"

run_on_exit() {
    echo ""
    if [ "$1" == "0" ]; then
        echo "Done :)"
    else
        echo "Done :("
    fi
    docker rm -f $container_name > /dev/null 2> /dev/null
}

# Make sure run_on_exit runs even when we encounter errors
trap "run_on_exit 1" ERR

# Set the output directory for the container build
docker_bazel_output_dir=/tmp/reproducible_build/$container_name

docker -D run -d -i \
--privileged \
-v /var/run/docker.sock:/var/run/docker.sock \
-v $docker_bazel_output_dir:/tmp/bazel_build_output \
--name $container_name \
$build_container_image_name

# Copy the scp source code into the build container
# The -L is important as we are copying from a symlink
docker cp -L $source_code_tar $container_name:/

# Extract the source code
docker exec $container_name tar -xf /source_code_tar.tar

# Set the build output directory
docker exec $container_name \
bash -c "echo 'startup --output_user_root=/tmp/bazel_build_output' >> /scp/.bazelrc"

# Build the container image
docker exec -w /scp $container_name \
bash -c "bazel build $build_flags --action_env=BAZEL_CXXOPTS=\"-std=c++17\" $args $container_tar_target_path"

# Change the build output directory permissions to the user running this script
user_id="$(id -u)"
docker exec $container_name chown -R $user_id:$user_id /tmp/bazel_build_output

echo $docker_bazel_output_dir

# Copy the container image to the output
cp $(find $docker_bazel_output_dir -name "${container_name_to_build}.tar") $output_tar
run_on_exit 0
