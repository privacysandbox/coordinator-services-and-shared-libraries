#!/usr/bin/env bash

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

set -eu

# Prints usage instructions
print_usage() {
  echo "Usage:  ./operator_onboarding.sh \\"
  printf "\t\t--coord_env=<environment> \\"
  printf "\n\t\t--pbs_auth_state_bucket=<bucket> \\"
  printf "\n\t\t--pbs_allowed_operators_remote_file_path=<path> \\"
  printf "\n\t\t--pbs_allowed_operators_local_file_path=<path> \\"
  printf "\n\t\t--pbs_auth_spanner_instance_id=<id> \\"
  printf "\n\t\t--pbs_auth_spanner_database_id=<id> \\"
  printf "\n\t\t--pbs_auth_table_name=<table> \\"
  printf "\n\t\t[--auto_approve=<true/false>]"
  printf "\n\n\t--coord_env                              - The coordinator environment to deploy e.g. environments_mp_primary/demo.\n"
  printf "\t--auto_approve                           - Whether to auto-approve the Terraform action. Default is false.\n"
  printf "\t--pbs_auth_state_bucket                  - The GCS bucket to store the PBS auth remote state.\n"
  printf "\t--pbs_allowed_operators_remote_file_path - The file path to store the PBS auth remote state.\n"
  printf "\t--pbs_allowed_operators_local_file_path  - The local file path for the desired PBS auth state.\n"
  printf "\t--pbs_auth_spanner_instance_id           - The PBS Auth table Spanner instance id.\n"
  printf "\t--pbs_auth_spanner_database_id           - The PBS Auth table Spanner database id.\n"
  printf "\t--pbs_auth_table_name                    - The PBS Auth table name.\n"
}

FUNCTION=$1
# parse arguments
for ARGUMENT in "$@"
  do
    case $ARGUMENT in
      --pbs_auth_state_bucket=*)
        PBS_AUTH_STATE_BUCKET=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      --pbs_allowed_operators_remote_file_path=*)
        PBS_ALLOWED_OPERATORS_REMOTE_FILE_PATH=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      --pbs_allowed_operators_local_file_path=*)
        PBS_ALLOWED_OPERATORS_LOCAL_FILE_PATH=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      --pbs_auth_spanner_instance_id=*)
        PBS_AUTH_SPANNER_INSTANCE_ID=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      --pbs_auth_spanner_database_id=*)
        PBS_AUTH_SPANNER_DATABASE_ID=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      --pbs_auth_table_name=*)
        PBS_AUTH_TABLE_NAME=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      --coord_env=*)
        COORD_ENV=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      --auto_approve=*)
        AUTO_APPROVE=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      help)
        print_usage
        exit 1
        ;;
      *)
        printf "ERROR: invalid argument $ARGUMENT\n"
        print_usage
        exit 1
        ;;
    esac
done

PARAMETERS=''

if [[ -n ${AUTO_APPROVE+x} ]]; then
    PARAMETERS="${PARAMETERS} --auto_approve=$AUTO_APPROVE "
fi

SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
ENV_DIR="$SCRIPT_DIR/../../$COORD_ENV"

if [ ! -d "$ENV_DIR" ]; then
  printf "Environment directory $ENV_DIR does not exist\n"
  exit 1
fi

# Deploy allowed operator group terraform
$SCRIPT_DIR/coordinator_deploy.sh \
--coord_env_path="$ENV_DIR" \
--coord_application="allowedoperatorgroup" \
$PARAMETERS

# Execute operator onboarding for PBS Auth
python3 $SCRIPT_DIR/add_allowed_identities.py \
  "$PBS_AUTH_STATE_BUCKET" \
  "$PBS_ALLOWED_OPERATORS_REMOTE_FILE_PATH" \
  "$PBS_ALLOWED_OPERATORS_LOCAL_FILE_PATH" \
  "$PBS_AUTH_SPANNER_INSTANCE_ID" \
  "$PBS_AUTH_SPANNER_DATABASE_ID" \
  "$PBS_AUTH_TABLE_NAME"
