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

# This script load and run the container image for SDK

# Fail on any error.
set -ex

# Bazel target path
build_target_path=$1
# Name of the bazel target
build_target_name=$2
# Folder on host machine to be mounted for socket communication. Only needed for
# outside tee
host_folder_mount=${3-/tmp}

reproducible_target="$build_target_name"_reproducible_container

# Build the image
bazel build "$build_target_path":"$reproducible_target"

# Load the image
docker load --input bazel-bin/"$build_target_path"/"$reproducible_target".tar

cp /etc/passwd /tmp/passwd.docker
grep $USER /tmp/passwd.docker || getent passwd | grep $USER >> /tmp/passwd.docker
docker_gid=$(getent group docker | cut -d: -f3)

# Start container.
# Need to set group and user to allow access /tmp file from outside container
docker run -it \
    -u=$(id -u):$docker_gid \
    -e USER=$USER \
    -e HOME=$HOME \
    -v /tmp/passwd.docker:/etc/passwd:ro \
    -v /etc/group:/etc/group:ro \
    -v "$HOME:$HOME:ro" \
    -v "$host_folder_mount":/tmp:rw \
    -v "$HOME"/.aws:/root/.aws:ro \
    -v "$HOME"/.config/gcloud:/root/.config/gcloud:ro \
    bazel/"$build_target_path":"$build_target_name"_container
