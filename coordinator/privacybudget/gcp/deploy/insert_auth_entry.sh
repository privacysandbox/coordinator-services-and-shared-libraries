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

# This script allows for inserting or updating entries to the PBS auth table.
# It is intended to be run locally.
#
# Args:
#   required:
#    --instance=<value> \ # The Spanner auth instance name.
#    --database=<value> \ # The Spanner auth database name.
#    --table_v2=<value> \ # The Spanner auth table v2 name
#    --account=<value> \ # The email of the user, group, or service account to authorize.
#    --site=<value> \ # The site value used by adtech
#    --project=<value> \ $ GCP project id of coordinator
#
# `./insert_auth_entry.sh help` for usage instructions.

set -euo pipefail

function insert_auth_table_v2_entry() {
    local sql_statement
    local record_count
    sql_statement="SELECT COUNT(*) AS Count FROM $database_table_v2_name WHERE AccountID='$account_id'"
    record_count=`gcloud spanner databases execute-sql $database_name --project=$project --instance=$database_instance_name --sql="$sql_statement" --format=json | jq -r .rows[0][0]`
    # Performs an insert if record does not exist.
    if [[ "$record_count" == "0" ]]; then
        (gcloud spanner databases execute-sql $database_name --project=$project --instance=$database_instance_name --sql="""INSERT INTO
          $database_table_v2_name (AccountId, AdtechSites)
        VALUES
          ('$account_id', ['$site'])
        THEN RETURN
          AccountId,
          AdtechSites;""")

    # Performs an update to the adtech site list if the account record exists.
    elif  [[ "$record_count" == "1" ]]; then
        (gcloud spanner databases execute-sql $database_name --project=$project --instance=$database_instance_name --sql="""UPDATE
          $database_table_v2_name
        SET
          AdtechSites=['$site']
        WHERE
          AccountId='$account_id'
        THEN RETURN
          AccountId,
          AdtechSites;""")
    fi
}

if [[ "$#" -lt 1 || $1 == "help" ]]; then
help_msg=$(cat <<-END
  \n
  To insert an authorized user, group, or service account into the PBS auth table:\n
  Provide the Spanner auth instance, database, and table names along with an email for authorization and the email's site.\n
  This script uses the configured region, and account credentials.\n
  Example:
  \n\n
  ./insert_auth_entry.sh --project=gcp_project_id --instance=auth_inst_name --database=auth_db_name --table_v2=auth_table_v2_name --account=authorized_group@googlegroups.com --site=https://pbsauth.com
  \n\n
  In the example above, the provided email would:\n
  1. be added to the auth table if it doesn't yet exist in the database.\n
  2. have its list of sites set to hold the provided site in the AuthorizationV2 table.
END
)
  echo -e $help_msg
  exit 1
fi

if [ "$#" -lt 5 ]; then
error_msg=$(cat <<-END
  Must provide all required inputs:\n
    --instance=<value>\n
    --database=<value>\n
    --table_v2=<value>\n
    --account=<value>\n
    --site=<value>\n
    --project=<value>\n
END
)
  echo -e $error_msg
  exit 1
fi

auto_approve=false

while [ $# -gt 0 ]; do
  case "$1" in
    --instance=*)
      database_instance_name="${1#*=}"
      ;;
    --database=*)
      database_name="${1#*=}"
      ;;
    --table_v2=*)
      database_table_v2_name="${1#*=}"
      ;;
    --account=*)
      account_id="${1#*=}"
      ;;
    --site=*)
      site="${1#*=}"
      ;;
    --project=*)
      project="${1#*=}"
      ;;
    *)
      printf "***************************\n"
      printf "* Error: Invalid argument.*\n"
      printf "***************************\n"
      exit 1
  esac
  shift
done

# Inserts or updates the entry for a provided account in the provided PBS auth table.
insert_auth_table_v2_entry
