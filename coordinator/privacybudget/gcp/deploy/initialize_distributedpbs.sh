#!/usr/bin/env bash

# Copyright 2025 Google LLC
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

# This script allows initializng the distributed PBS environment
# It is intended to be run locally within the deployment directory,
# adjacent to environments_mp_primary and environments_mp_secondary.
#
# The optional args are there to allow overriding the terraform output lookups
# Terraform will not be invoked if all the optional args are supplied
#
# Args:
#   required:
#    --environment=<value> \ # The environment directory name
#    --coordinator=<value> \ # The coordinator name, use to complete 'environments_mp_' directory name
#   optional:
#    --gcp-project-id=<value> \ # The gcp project containing the PBS DB
#    --pbs-database-instance=<value> \ # The of the PBS spanner instance
#    --pbs-database=<value> \ # The name of the PBS spanner database
#    --pbs-database-table=<value> \ # the name of the PBS spanner table
# See `./initialize_distributedpbs.sh help` for usage instructions.

set -eu -o pipefail

show_help() {
  help_msg=$(cat <<-END
    Initializes the PBS configuration when provided the environment name and the coordinator name
    \n\n
    Example:\n
    ./initialize_distributedpbs.sh --environment=staging --coordinator=primary\n
    \n
    In the example above, the environment initialized would be:\n
    environments_mp_primary\n
    \t|__staging\n
    \t\t|__distributedpbs_base\n
    \t\t|__distributedpbs_application\n
    \n
    Where the following would occur:\n
    1. Any schema updates to the PBS table will be made.\n
END
  )
  echo -e ${help_msg}
}

#######################################
# Lookup terraform outputs relating to PBS
# Arguments:
#   $1 - full path to directory containing distributedpbs_(application|base) folders
#######################################
function lookup_tf_outputs() {
  local environment_dir="$1"
  terraform -chdir="${environment_dir}/distributedpbs_application" init -input=false
  terraform -chdir="${environment_dir}/distributedpbs_base" init -input=false

  echo "Collecting application and base layer outputs."
  # Captures this deployment's outputs to be used later
  pbs_database_instance_name="$(terraform -chdir="${environment_dir}/distributedpbs_application" output -raw pbs_spanner_instance_name)"
  pbs_database_name="$(terraform -chdir="${environment_dir}/distributedpbs_application" output -raw pbs_spanner_database_name)"
  pbs_spanner_budget_key_table_name="$(terraform -chdir="${environment_dir}/distributedpbs_application" output -raw pbs_spanner_budget_key_table_name)"
  gcp_project_id="$(terraform -chdir="${environment_dir}/distributedpbs_base" output -raw project_id)"
}

#######################################
# Update PBS spanner table with changes relating to the protocol buffer column
# Arguments:
#   $1 - gcp project id of the coordinator project
#   $2 - name of pbs database
#   $3 - name of pbs database instance
#   $4 - name of pbs table
#######################################
function update_pbs_spanner_proto_bundle_and_column() {
  local proto_bundle_file_path
  local has_proto_bundle
  local all_bundles
  local exit_status

  local gcp_project_id="$1"
  local pbs_database_name="$2"
  local pbs_database_instance_name="$3"
  local pbs_spanner_budget_key_table_name="$4"

  echo "Updating PBS Spanner Proto bundle..."
  proto_bundle_file_path="./dist/budget_value_proto-descriptor-set.proto.bin"
  if [[ ! -f "${proto_bundle_file_path}" ]]; then
      echo "WARNING: PBS proto bundle [${proto_bundle_file_path}] does not exist."
      return
  fi

  echo "Using the following parameters: "
  echo "gcp_project_id: ${gcp_project_id}"
  echo "pbs_database: ${pbs_database_name}"
  echo "pbs_database_instance: ${pbs_database_instance_name}"
  echo "pbs_spanner_budget_key_table: ${pbs_spanner_budget_key_table_name}"

  # Check whether the proto bundle already exists in the PBS Spanner database.
  has_proto_bundle="$(gcloud spanner databases execute-sql "${pbs_database_name}" \
  --instance="${pbs_database_instance_name}" \
  --sql='SELECT COUNT(PROTO_BUNDLE) as proto_bundle_count FROM `INFORMATION_SCHEMA`.`SCHEMATA` WHERE PROTO_BUNDLE IS NOT NULL' \
  --project="${gcp_project_id}" \
  --quiet | sed -n '2p')"
  exit_status=$?

  if [[ "${exit_status}" -ne 0 ]]; then
    echo "Failed to read information schema for BudgetKey Spanner DB." >&2
    exit "${exit_status}"
  fi

  # All proto messages that need to be used by PBS Spanner database.
  all_bundles="
  (
    \`privacy_sandbox_pbs.BudgetValue\`
  )
  "

  if [[ "${has_proto_bundle}" = "1" ]]; then
    echo "Proto bundle found in BudgetKey DB. Updating proto bundle..."
    gcloud spanner databases ddl update "${pbs_database_name}" --instance="${pbs_database_instance_name}" \
    --ddl="ALTER PROTO BUNDLE UPDATE ${all_bundles};" \
    --proto-descriptors-file="${proto_bundle_file_path}" \
    --project="${gcp_project_id}" \
    --quiet
    exit_status=$?
  else
    echo "No proto bundle in BudgetKey DB. Creating proto bundle..."
    gcloud spanner databases ddl update "${pbs_database_name}" --instance="${pbs_database_instance_name}" \
    --ddl="CREATE PROTO BUNDLE ${all_bundles};" \
    --proto-descriptors-file="${proto_bundle_file_path}" \
    --project="${gcp_project_id}" \
    --quiet
    exit_status=$?
  fi

  if [[ "${exit_status}" -ne 0 ]]; then
    echo "Failed to create or update proto bundle for BudgetKey DB." >&2
    exit "${exit_status}"
  fi

  echo "Creating ValueProto column if it does not exist in PBS Spanner database..."
  gcloud spanner databases ddl update "${pbs_database_name}" --instance="${pbs_database_instance_name}" \
  --ddl="ALTER TABLE ${pbs_spanner_budget_key_table_name} ADD COLUMN IF NOT EXISTS ValueProto privacy_sandbox_pbs.BudgetValue;" \
  --project="${gcp_project_id}" \
  --quiet
}


main() {
  local environment
  local coordinator

  local gcp_project_id
  local pbs_database_name
  local pbs_database_instance_name
  local pbs_database_table_name

  local gcp_project_id_arg=""
  local pbs_database_arg=""
  local pbs_database_instance_arg=""
  local pbs_database_table_arg=""

  if [[ "$#" -lt 1 || $1 == "help" ]]; then
    show_help
    exit 1
  fi

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --environment=*)
        environment="${1#*=}"
        ;;
      --coordinator=*)
        coordinator="${1#*=}"
        ;;
      --gcp-project-id=*)
        gcp_project_id_arg="${1#*=}"
        ;;
      --pbs-database=*)
        pbs_database_arg="${1#*=}"
        ;;
      --pbs-database-instance=*)
        pbs_database_instance_arg="${1#*=}"
        ;;
      --pbs-database-table=*)
        pbs_database_table_arg="${1#*=}"
        ;;
      *)
        printf "***************************\n"
        printf "* Error: Invalid argument.*\n"
        printf "***************************\n"
        exit 1
    esac
    shift
  done

  if [[ -z ${gcp_project_id_arg} || -z "${pbs_database_arg}" ||
        -z "${pbs_database_instance_arg}" ||
        -z "${pbs_database_table_arg}" ]]; then
    lookup_tf_outputs "./environments_mp_${coordinator}/${environment}"
  fi;

  update_pbs_spanner_proto_bundle_and_column \
    "${gcp_project_id_arg:-${gcp_project_id}}" \
    "${pbs_database_arg:-${pbs_database_name}}" \
    "${pbs_database_instance_arg:-${pbs_database_instance_name}}" \
    "${pbs_database_table_arg:-${pbs_spanner_budget_key_table_name}}"
}

main "$@"
