#!/bin/bash
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


set -euo pipefail

eval "$(cat $(bazel info workspace)/cc/tools/build/build_container_params.bzl | tr -d ' ')"
build_container_tag="$CC_BUILD_CONTAINER_TAG"

if [ "$#" -ne 2 ]; then
  info_msg=$(cat <<-END
    Must provide all input variables. Switches are:\n
      --bazel_command=<value>\n
      --bazel_output_directory=<value>\n
END
)
  echo -e $info_msg
  exit 1
fi

while [ $# -gt 0 ]; do
  case "$1" in
    --bazel_command=*)
      bazel_command="${1#*=}"
      ;;
    --bazel_output_directory=*)
      bazel_output_directory="${1#*=}"
      ;;
    *)
      printf "***************************\n"
      printf "* Error: Invalid argument.*\n"
      printf "***************************\n"
      exit 1
  esac
  shift
done

# Create the output directory if it does not exist
[ ! -d "$bazel_output_directory" ] && mkdir -p $bazel_output_directory

[[ $bazel_command != *"//cc"* ]] && \
  echo "" && \
  echo "ERROR: The path in the command [$bazel_command] does not start with the absolute //cc bazel target path." && \
  exit 1

repo_top_level_dir=$(git rev-parse --show-toplevel)
timestamp=$(date "+%Y%m%d-%H%M%S%N")
run_version="scp_cc_build_$timestamp"

run_on_exit() {
  # Change the build output directory permissions back to the user running this script
  user_id="$(id -u)"
  docker exec $run_version chown -R $user_id:$user_id /tmp/bazel_build_output

  echo ""
  if [ "$1" == "0" ]; then
    echo "Done :)"
  else
    echo "Done :("
  fi

  docker rm -f $run_version > /dev/null 2> /dev/null &
}

# Make sure run_on_exit runs even when we encounter errors
trap "run_on_exit 1" ERR

# Pull and load build image
container_image_name="bazel/cc/tools/build:prebuilt_cc_build_container_image"
bazel build //cc/tools/build:prebuilt_cc_build_container_image.tar
docker load < bazel-bin/cc/tools/build/prebuilt_cc_build_container_image.tar

# Run the build container
# --privileged: Needed to access the docker socket on the host machine
# --network host: We want the tests to be executable the same way they would be on the host machine
docker -D run -d -i \
    --privileged \
    --network host \
    --ipc host \
    -v $bazel_output_directory/$run_version:/tmp/bazel_build_output \
    -v /var/run/docker.sock:/var/run/docker.sock \
    -v /tmp:/tmp:rw \
    --name $run_version \
    $container_image_name

# Copy the scp source code into the build container
docker cp $repo_top_level_dir $run_version:/scp

# Remove the bazel artifacts from the copied files if they exist
docker exec $run_version \
  bash -c "([[ $(find $repo_top_level_dir -name 'bazel-*' | wc -l) -gt 0 ]] && rm -rf /scp/bazel-*) || true"

# Change the ownership of the copied files to the root user within the build container
docker exec $run_version chown -R root:root /scp

# Set the build output directory
docker exec $run_version \
  bash -c "echo 'startup --output_user_root=/tmp/bazel_build_output' >> /scp/.bazelrc"

# Execute the command
docker exec -w /scp $run_version bash -c "$bazel_command"

run_on_exit 0
