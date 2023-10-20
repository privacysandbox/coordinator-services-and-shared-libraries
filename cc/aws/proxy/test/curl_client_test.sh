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

# This file, simulates what an application would do inside enclave. Here we use
# curl to access www.google.com, though preloaded library and proxy.

set -euo pipefail

proxy_path=$1
preload_path=$2
proxify_path=$3

if [[ ! -x $proxy_path ]]; then
  echo "Proxy: ${proxy_path} is not accessible"
  exit 1
fi

if [[ ! -f $preload_path ]]; then
  echo "Preload lib: ${preload_path} is not accessible"
  exit 1
fi

export PROXY_PARENT_PORT=8888

# Run the proxy, get pid.
$proxy_path -p $PROXY_PARENT_PORT &
proxy_pid=$!
sleep 1

export RES_OPTIONS="use-vc"
# Use loopback address 1 as proxy address.
export PROXY_PARENT_CID=1

# TODO: change this to not depend on google.com
google_com_len=$(LD_PRELOAD="$preload_path" curl -s https://www.google.com | wc -c)
if [[ $google_com_len -lt 4096 ]]; then
  echo "Preload lib: google.com returned less than 4KiB"
  kill $proxy_pid
  exit 1
fi

# Test proxify
google_com_len=$($proxify_path curl -s https://www.google.com | wc -c)
kill $proxy_pid
if [[ $google_com_len -lt 4096 ]]; then
  echo "Proxify: google.com returned less than 4KiB"
  exit 1
fi
