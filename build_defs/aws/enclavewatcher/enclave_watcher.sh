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

#
# Starts or stops the worker enclave on the system, notifying systemd of status
# updates. Assumes at-most one enclave will be running and that if one enclave
# is running, the service is running.
#
# Infers how many resources should be allocated to the enclave using the
# allocator configuration file (configurable by setting ALLOCATOR_YAML_PATH).
# Assumes PyYAML is available on the system for parsing that file.
#
# Usage:
#   enclave_watcher.sh (start|stop)
set -euo pipefail

# Allow values to be overriden with environment variables.
ENCLAVE_PATH="${ENCLAVE_PATH:-/opt/google/scp/enclave.eif}"
ALLOCATOR_YAML_PATH="${ALLOCATOR_YAML_PATH:-/etc/nitro_enclaves/allocator.yaml}"
ENABLE_ENCLAVE_DEBUG_MODE="${ENABLE_ENCLAVE_DEBUG_MODE:-0}"

ENCLAVE_DEBUG_MODE_FLAG=""

if [[ $ENABLE_ENCLAVE_DEBUG_MODE != 0 ]]; then
  ENCLAVE_DEBUG_MODE_FLAG="--debug-mode"
fi

# Perodically notify systemd that the enclave is alive so long as
# describe-enclaves says there is a running enclave
watch_enclave() {
  while true ; do
    # TODO(b/205058644): Investigate alternative health check mechanisms that
    # can provide a better signal than "describe-enclaves" (e.g. /streamz
    # polling)
    local result=$(nitro-cli describe-enclaves | jq '.[0].State' -r)

    if [[ $result != "RUNNING" ]]; then
      echo "Invalid status: ${result}"
      systemd-notify ERRNO=1
      exit 1
    fi
    systemd-notify WATCHDOG=1
    sleep 10;
  done
}

# Starts the enclave with parameters derived from the allocator yaml file
start_enclave() {
  local allocator=$(get_allocator_config)
  local cpu=$(echo "$allocator" | jq -r .cpu_count)
  local memory=$(echo "$allocator" | jq -r .memory_mib)

  nitro-cli run-enclave --cpu-count="${cpu}" --memory="${memory}" --eif-path "${ENCLAVE_PATH}" $ENCLAVE_DEBUG_MODE_FLAG
  systemd-notify READY=1
}

# Stops the first enclave running on the system (assumes only one will be running).
stop_enclave() {
  nitro-cli terminate-enclave --enclave-id $(nitro-cli describe-enclaves | jq -r '.[].EnclaveID')
}

# Prints the YAML allocator config as JSON for being parsed by jq.
get_allocator_config() {
  python -c 'import sys, yaml, json; json.dump(yaml.load(sys.stdin), sys.stdout)' < "${ALLOCATOR_YAML_PATH}"
}

if [[ $# -ne 1 ]]; then
   echo "Error: expected 1 argument and got $#";
   exit 1;
fi

if [[ $1 == "start" ]]; then
  echo "starting enclave"
  start_enclave
  echo "watching enclave"
  watch_enclave
elif [[ $1 == "stop" ]]; then
  echo "stopping enclave"
  stop_enclave
else
  echo "Error: expected (start|stop) and got $1"
fi
