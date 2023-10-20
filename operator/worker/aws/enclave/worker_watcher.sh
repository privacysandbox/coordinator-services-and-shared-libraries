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
#   worker_watcher.sh (start|stop)
set -euo pipefail

# Allow values to be overriden with environment variables.
ENCLAVE_PATH="${ENCLAVE_PATH:-/opt/google/worker/enclave.eif}"
ALLOCATOR_YAML_PATH="${ALLOCATOR_YAML_PATH:-/etc/nitro_enclaves/allocator.yaml}"
ENCLAVE_FAILURE_COUNT_FILE="/tmp/enclave_start_failure_count"
ENABLE_WORKER_DEBUG_MODE="${ENABLE_WORKER_DEBUG_MODE:-0}"
WORKER_DEBUG_MODE_FLAG=""

if [[ $ENABLE_WORKER_DEBUG_MODE != 0 ]]; then
  WORKER_DEBUG_MODE_FLAG="--debug-mode"
fi

# This function checks essential permissions required to run the worker. This
# allows us to see the config/permission errors prior to starting enclave, where
# the errors are invisible without debug-mode. Exits with non-zero code upon
# encountering a config or permission error
check_permissions() {
  echo "Verifying access to EC2 metadata API and initializing a session..."
  local token=$(curl -s --fail-with-body -X PUT http://169.254.169.254/latest/api/token -H "X-aws-ec2-metadata-token-ttl-seconds: 120")
  echo "Acquired session token."
  local region=$(curl -s --fail-with-body http://169.254.169.254/latest/meta-data/placement/region -H "X-aws-ec2-metadata-token: ${token}")
  echo "region=${region}"
  local instance_id=$(curl -s --fail-with-body http://169.254.169.254/latest/meta-data/instance-id -H "X-aws-ec2-metadata-token: ${token}")
  echo "instance_id=${instance_id}"
  local role=$(curl -s --fail-with-body http://169.254.169.254/latest/meta-data/iam/security-credentials/ -H "X-aws-ec2-metadata-token: ${token}")
  echo "Verifying IAM role attached..."
  echo "role=${role}"
  # Now check if we can get the tags of this instance
  echo "Checking instance tags..."
  local tags="$(aws --region $region ec2 describe-tags --filters Name=resource-id,Values=${instance_id})"
  local tags_kv=$(echo "${tags}" | jq "[.Tags[] | {key:.Key, value:.Value}] | from_entries")
  echo "========================================================"
  echo "${tags_kv}"
  echo "========================================================"
  echo "Verifying environment tag present..."
  local environment=$(echo "${tags_kv}" | jq -r ".environment")
  echo "environment=${environment}"
  echo "Checking SSM permissions..."
  local params=("sqs_queue_url" "dynamodb_metadatadb_table_name" "max_job_num_attempts" \
                "max_job_processing_time_seconds" "assume_role_arn" "kms_key_arn" "scale_in_hook")
  local params_len_expected="${#params[@]}"
  local params_joined=$(printf " /aggregate-service/${environment}/%s" "${params[@]}")
  local ssm_resp=$(aws --region ${region} ssm get-parameters --names ${params_joined})
  local param_len=$(echo "$ssm_resp" | jq ".Parameters | length")
  if [[ $param_len -ne $params_len_expected ]]; then
    echo "Error: Not all params can be accessed."
    echo "Params required: $params_joined"
    echo "Response:"
    echo "${ssm_resp}"
    exit 1
  fi
  echo "Done!"
}

# Periodically notify systemd that the enclave is alive so long as
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

wait_for_instance_replacement() {
  while true ; do
    echo "Waiting for Unhealthy instance replacement."
    systemd-notify WATCHDOG=1
    sleep 30;
  done
}

handle_enclave_startup_failure() {
  local enclave_failure_count=0
  # Increment enclave start failure count.
  if [ -e ${ENCLAVE_FAILURE_COUNT_FILE} ]; then
    enclave_failure_count=$(cat ${ENCLAVE_FAILURE_COUNT_FILE})
  fi
  enclave_failure_count=$((enclave_failure_count + 1))
  echo ${enclave_failure_count} > ${ENCLAVE_FAILURE_COUNT_FILE}

  # If the enclave startup failure count is greater or equal to 5, then
  # mark the EC2 instance Unhealthy for replacement.
  if (( enclave_failure_count >= 5 )); then
    local token=$(curl -s --fail-with-body -X PUT http://169.254.169.254/latest/api/token -H "X-aws-ec2-metadata-token-ttl-seconds: 120")
    echo "Acquired session token."
    local region=$(curl -s --fail-with-body http://169.254.169.254/latest/meta-data/placement/region -H "X-aws-ec2-metadata-token: ${token}")
    local instance_id=$(curl -s --fail-with-body http://169.254.169.254/latest/meta-data/instance-id -H "X-aws-ec2-metadata-token: ${token}")
    echo "Setting instance ${instance_id} in ${region} to Unhealthy."
    if aws autoscaling set-instance-health --instance-id "${instance_id}" --region "${region}" --health-status Unhealthy; then
      echo "Marked instance Unhealthy and waiting for instance replacement."
      wait_for_instance_replacement
    else
      echo "Failed to mark instance unhealthy for replacement."
      sleep 30
      systemd-notify ERRNO=1
      exit 1
    fi
  fi
}

# Starts the enclave with parameters derived from the allocator yaml file
start_enclave() {
  local allocator=$(get_allocator_config)
  local cpu=$(echo "$allocator" | jq -r .cpu_count)
  local memory=$(echo "$allocator" | jq -r .memory_mib)

  if ! nitro-cli run-enclave --cpu-count="${cpu}" --memory="${memory}" --eif-path "${ENCLAVE_PATH}" $WORKER_DEBUG_MODE_FLAG; then
    handle_enclave_startup_failure
    sleep 10
    systemd-notify ERRNO=1
    exit 1
  fi
  echo 0 > ${ENCLAVE_FAILURE_COUNT_FILE}
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

# Fetches a numeric tag value and prints its value to stdout.
# Returns non-zero if the request fails or the value isn't a valid number.
fetch_tag_value() {
  local region=$1
  local instance_id=$2
  local tag_key=$3

  aws ec2 describe-tags --region "${region}" --filters \
    "Name=resource-id,Values=${instance_id}" \
    "Name=key,Values=${tag_key}" \
    | jq -r '.Tags[0].Value | tonumber'
}

# Checks this EC2 instance's tags and updates allocator.yml -- restarting
# nitro-enclaves-allocator if necessary.
update_allocator_config() {
  echo "Updating allocator.yaml"

  local token
  token=$(curl -s --fail-with-body -X PUT http://169.254.169.254/latest/api/token -H "X-aws-ec2-metadata-token-ttl-seconds: 120")
  echo "Acquired session token."
  local region
  region=$(curl -s --fail-with-body http://169.254.169.254/latest/meta-data/placement/region -H "X-aws-ec2-metadata-token: ${token}")
  local instance_id
  instance_id=$(curl -s --fail-with-body http://169.254.169.254/latest/meta-data/instance-id -H "X-aws-ec2-metadata-token: ${token}")
  # Default to 2 vCPU / ~7 GB if tags inaccessible or malformed. Should support
  # all instance shapes that support enclaves (c5.xlarge and larger).
  local num_cpus
  cpu_count=$(fetch_tag_value "${region}" "${instance_id}" "enclave_cpu_count" \
    || echo -n 2)
  local memory_mib
  memory_mib=$(fetch_tag_value "${region}" "${instance_id}" "enclave_memory_mib" \
    || echo -n 7000)

  sed -i.bak "${ALLOCATOR_YAML_PATH}" \
    -e "s/memory_mib:.*/memory_mib: ${memory_mib}/" \
    -e "s/cpu_count:.*/cpu_count: ${cpu_count}/"

  if ! cmp $ALLOCATOR_YAML_PATH $ALLOCATOR_YAML_PATH.bak; then
    echo "allocator config changes detected, restarting allocator"
    systemctl restart nitro-enclaves-allocator.service
  else
    echo "no allocator config changes detected, continuing"
  fi
}

if [[ $# -ne 1 ]]; then
   echo "Error: expected 1 argument and got $#";
   exit 1;
fi

if [[ $1 == "start" ]]; then
  echo "starting enclave"
  check_permissions
  update_allocator_config
  start_enclave
  echo "watching enclave"
  watch_enclave
elif [[ $1 == "stop" ]]; then
  echo "stopping enclave"
  stop_enclave
else
  echo "Error: expected (start|stop) and got $1"
fi
