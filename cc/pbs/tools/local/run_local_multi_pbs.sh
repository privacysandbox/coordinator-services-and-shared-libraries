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

SERVER_ONE_HOST_PORT="${SERVER_ONE_HOST_PORT:-9090}"
SERVER_ONE_HEALTH_PORT="${SERVER_ONE_HEALTH_PORT:-9091}"
SERVER_TWO_HOST_PORT="${SERVER_TWO_HOST_PORT:-9092}"
SERVER_TWO_HEALTH_PORT="${SERVER_TWO_HEALTH_PORT:-9093}"

IO_ASYNC_EXEC_QUEUE_SIZE="${IO_ASYNC_EXEC_QUEUE_SIZE:-10000}"
IO_ASYNC_EXEC_THREADS="${IO_ASYNC_EXEC_THREADS:-10}"
ASYNC_EXEC_QUEUE_SIZE="${ASYNC_EXEC_QUEUE_SIZE:-10000}"
ASYNC_EXEC_THREADS="${ASYNC_EXEC_THREADS:-10}"
TX_MANAGER_CAPACITY="${TX_MANAGER_CAPACITY:-20000}"
HTTP2_SERVER_THREADS="${HTTP2_SERVER_THREADS:-10}"


function run_server() {
    local container_image_name="bazel/cc/pbs/deploy/pbs_server/build_defs:pbs_container_local"
    local coordinator_role=$1
    local host_port=$2
    local health_port=$3
    local remote_host_port=$4

    local bucket_path="$PWD/$coordinator_role"-bucket
    local log_file="pbs-$coordinator_role.log"

    # Create the file that will be mounted on the container so that logs are printed here.
    [ ! -f "$log_file" ] && touch "$log_file"
    # Create the directory for the bucket if it doesn't exist
    [ ! -d "$bucket_path" ] && mkdir "$bucket_path"

    docker -D run -d -i \
    --privileged \
    --network host \
    --ipc host \
    --name "$coordinator_role"-local-pbs \
    -v "$PWD/$log_file":/var/log/syslog \
    -v "$PWD:$PWD" \
    --env google_scp_core_cloud_region=us-east \
    --env google_scp_aggregated_metric_interval_ms=5000 \
    --env google_scp_pbs_journal_service_bucket_name=$bucket_path \
    --env google_scp_pbs_budget_key_table_name=budget \
    --env google_scp_pbs_partition_lock_table_name=lock \
    --env google_scp_pbs_host_port="$host_port" \
    --env google_scp_pbs_health_port="$health_port" \
    --env google_scp_pbs_http2_server_use_tls=false \
    --env google_scp_pbs_partition_lease_duration_in_seconds=5 \
    --env google_scp_pbs_io_async_executor_queue_size=$IO_ASYNC_EXEC_QUEUE_SIZE \
    --env google_scp_pbs_io_async_executor_threads_count=$IO_ASYNC_EXEC_THREADS \
    --env google_scp_core_http2server_threads_count=$HTTP2_SERVER_THREADS \
    --env google_scp_pbs_transaction_manager_capacity=$TX_MANAGER_CAPACITY \
    --env google_scp_pbs_async_executor_threads_count=$ASYNC_EXEC_THREADS \
    --env google_scp_pbs_async_executor_queue_size=$ASYNC_EXEC_QUEUE_SIZE \
    --env google_scp_pbs_journal_service_partition_name=00000000-0000-0000-0000-000000000000 \
    --env google_scp_pbs_host_address=0.0.0.0 \
    --env google_scp_pbs_remote_claimed_identity=remote \
    --env google_scp_pbs_metrics_namespace=namespace \
    --env google_scp_pbs_metrics_batch_push_enabled=false \
    --env google_scp_pbs_metrics_batch_time_duration_ms=1000 \
    --env google_scp_pbs_multi_instance_mode_disabled=true \
    --env google_scp_pbs_remote_host_address=http://127.0.0.1:$remote_host_port \
    --env google_scp_pbs_remote_cloud_region=us-east \
    --env google_scp_pbs_remote_claimed_identity=remote-id \
    --env google_scp_pbs_remote_assume_role_arn=remote-arn \
    --env google_scp_pbs_remote_assume_role_external_id=ext-id \
    "$container_image_name"
}

delete_artifacts=false
while [ $# -gt 0 ]; do
  case "$1" in
    --delete_artifacts*)
      delete_artifacts=true
      ;;
    *)
      printf "***************************\n"
      printf "* Error: Invalid argument.*\n"
      printf "***************************\n"
      exit 1
  esac
  shift
done

repo_top_level_dir=$(git rev-parse --show-toplevel)

"$repo_top_level_dir"/cc/tools/build/local/start_container_local.sh
"$repo_top_level_dir"/cc/tools/build/local/bazel_build_within_container.sh \
    --bazel_command="bazel build --//cc/pbs/pbs_server/src/pbs_instance:platform=local //cc/pbs/deploy/pbs_server/build_defs:pbs_container_local.tar"

docker load < "$repo_top_level_dir/bazel-bin/cc/pbs/deploy/pbs_server/build_defs/pbs_container_local.tar"

if [ "$delete_artifacts" == "true" ]; then
    # Remove existing local artifacts
    echo "Removing local logs and files. This requires elevated permissions."
    sudo rm -rf *-bucket *.log
fi

run_server "a" "$SERVER_ONE_HOST_PORT" "$SERVER_ONE_HEALTH_PORT" "$SERVER_TWO_HOST_PORT"
run_server "b" "$SERVER_TWO_HOST_PORT" "$SERVER_TWO_HEALTH_PORT" "$SERVER_ONE_HOST_PORT"

echo ""
echo "PBS1 Host:   http://127.0.0.1:$SERVER_ONE_HOST_PORT"
echo "PBS1 Health: http://127.0.0.1:$SERVER_ONE_HEALTH_PORT"
echo ""
echo "PBS2 Host:   http://127.0.0.1:$SERVER_TWO_HOST_PORT"
echo "PBS2 Health: http://127.0.0.1:$SERVER_TWO_HEALTH_PORT"
