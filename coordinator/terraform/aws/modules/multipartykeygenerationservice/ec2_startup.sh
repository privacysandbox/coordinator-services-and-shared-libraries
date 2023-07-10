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
# Perform actions to run the aggregation service during first boot of ec2 instance.
set -euxo pipefail

systemctl start docker
systemctl restart nitro-enclaves-allocator.service

# This script runs in the boot phase in which environment variables are not set.
# This variable is needed to run nitro-cli build-enclave.
export NITRO_CLI_ARTIFACTS="/tmp/enclave-images/"

echo "Starting the proxy"
# AMI image being used has proxy in this location (home directory of ec2-user).
cd /home/ec2-user
pwd
# TODO: Set this up using systemd instead of as a startup script.
./proxy &
# Sleep here to allow time for proxy to bind to ports
sleep 5
pwd

# Add "--debug-mode" flag for debugging enclaves.
nitro-cli run-enclave --cpu-count 2 --memory 16384 --eif-path ./worker.eif
