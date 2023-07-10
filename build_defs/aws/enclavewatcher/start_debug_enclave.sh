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


# Script to help debug worker errors by starting a new enclave process in debug
# mode (note: preventing binary attestation) and immediately attaching a
# console. To be used manually.
#
# Useful for debugging the enclave when it fails to fully initialize and is in a
# flapping state (being restarted by systemd) by attaching a console immediately
# after starting the worker.
#
# Assumes at most one enclave is running on the system. Stops the enclave watcher
# process if it is running and does not restart it automatically.

set -euo pipefail

if [[ $EUID -ne 0 ]]; then
  echo "[Error]: Must be run as root."
  exit 1
fi

echo "[Info]: Stopping the enclave watcher"

set -x
# Stop enclave_watcher if it's currently running/restarting the enclave.
systemctl stop enclave-watcher

# Check for any other lingering enclaves  (e.g. started by this script)
EXISTING_ENCLAVE=$(nitro-cli describe-enclaves | jq '.[0].EnclaveID // empty' -r)

if [[ ! -z $EXISTING_ENCLAVE ]]; then
  echo "[Info]: Found running enclave, terminating it."
  nitro-cli terminate-enclave --enclave-id $EXISTING_ENCLAVE
fi

# Start the enclave in debug-mode.
nitro-cli run-enclave --cpu-count=2 --memory=7000 --eif-path=/opt/google/scp/enclave.eif --debug-mode

# Immedately attach a console to the first enclave (assuming the previous command succeeded).
nitro-cli console --enclave-id $(nitro-cli describe-enclaves | jq .[0].EnclaveID -r)

set +x

echo "[Info]: Run `systemctl start enclave-watcher` to re-enable the enclave watcher."
