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

if [ "$#" -ne 1 ]; then
  info_msg=$(cat <<-END
    Must provide all input variables. Switches are:\n
      --bazel_command=<value>\n
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
    *)
      printf "***************************\n"
      printf "* Error: Invalid argument.*\n"
      printf "***************************\n"
      exit 1
  esac
  shift
done

docker exec \
  -e AWS_ACCESS_KEY_ID=$AWS_ACCESS_KEY_ID \
  -e AWS_SECRET_ACCESS_KEY=$AWS_SECRET_ACCESS_KEY \
  -e AWS_SESSION_TOKEN=$AWS_SESSION_TOKEN \
  $build_container_tag bash -c "$bazel_command"
