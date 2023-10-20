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

eval "$(cat $(dirname $0)/../build_container_params.bzl | tr -d ' ')"
build_container_tag="$CC_BUILD_CONTAINER_TAG"

repo_top_level_dir=$(git rev-parse --show-toplevel)
user_id="$(id -u)"

if [ "$(docker ps -q -f name=$build_container_tag)" ]; then
    echo "Container - $build_container_tag is already started."
    exit 0
fi

# Pull and load the build container
container_image_name="bazel/cc/tools/build:prebuilt_cc_build_container_image"
bazel build //cc/tools/build:prebuilt_cc_build_container_image.tar
docker load < $repo_top_level_dir/bazel-bin/cc/tools/build/prebuilt_cc_build_container_image.tar

cp /etc/passwd /tmp/passwd.docker
grep $USER /tmp/passwd.docker || getent passwd | grep $USER >> /tmp/passwd.docker
docker_gid=$(getent group docker | cut -d: -f3)

# Run the build container
# --network host: We want the tests to be executable the same way they would be on the host machine
# -u=$user_id: Use the same UID as the runner to run the container
# -e USER=$USER User the same user name as the runner
# -e HOME=$HOME: Set $HOME to bazel output directory
# -v /etc/passwd:/etc/passwd:ro Use the same user set as host machine
# -v /etc/group:/etc/group:ro Use the same group set as host machine
# -v $repo_top_level_dir:$repo_top_level_dir Mount repo
# -v /var/run/docker.sock:/var/run/docker.sock: Mount docker.sock so it can be used inside container
docker -D run -d -i \
    --privileged \
    --network host \
    --ipc host \
    -u=$(id -u):$docker_gid \
    -e USER=$USER \
    -e HOME=$HOME \
    -v /tmp/passwd.docker:/etc/passwd:ro \
    -v /etc/group:/etc/group:ro \
    -v "$HOME:$HOME" \
    -v "$repo_top_level_dir:$repo_top_level_dir" \
    -v /var/run/docker.sock:/var/run/docker.sock \
    -w "$repo_top_level_dir" \
    -v /tmp:/tmp:rw \
    --name $build_container_tag \
    $container_image_name

echo ""
echo "Container - $build_container_tag is running now :)"
