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


eval "$(cat $(dirname $0)/../build_container_params.bzl | tr -d ' ')"
build_container_tag="$CC_BUILD_CONTAINER_TAG"

if [ "$(docker ps -qa -f name=$build_container_tag)" ]; then
    echo "Found container - $build_container_tag. Start removing:"

    docker rm -f $build_container_tag

    sudo chmod 660 /var/run/docker.sock

    echo ""
    echo "Container - $build_container_tag is stopped and removed."
else
    echo "Container - $build_container_tag does not exist!"
fi
