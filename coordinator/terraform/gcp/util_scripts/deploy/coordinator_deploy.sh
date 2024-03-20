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

ACTION="apply"
AUTO_APPROVE="false"

# Prints usage instructions
print_usage() {
  echo "Usage:  ./coordinator_deploy.sh --coord_env_path=<environment_path> --coord_application=<application> [--action=<action>] [--auto_approve=<true/false>]"
  printf "\n\t--coord_env_path     - The absolute path of the coordinator environment to deploy.\n"
  printf "\t--coord_application - The coordinator application to deploy e.g. mpkhs_primary."
  printf "\t--action            - Terraform action to take (apply|destroy|plan). Default is apply.\n"
  printf "\t--auto_approve      - Whether to auto-approve the Terraform action. Default is false.\n"
}

deploy-coordinator-terraform() {
  AUTO_APPROVE_OPTION=""

  if [[ $AUTO_APPROVE == "true" && $ACTION != "plan" ]]; then
    AUTO_APPROVE_OPTION=" -auto-approve "
  fi

  terraform -chdir="$ENV_DIR/$COORD_APPLICATION" init || { echo 'Failed to init operator terraform.' ; exit 1; }
  terraform -chdir="$ENV_DIR/$COORD_APPLICATION" $ACTION $AUTO_APPROVE_OPTION || { echo "Failed to apply $ACTION coordinator terraform." ; exit 1; }
}

if [ $# -eq 0 ]
  then
    printf "ERROR: Coordinator environment and application must be provided.\n"
    exit 1
fi

FUNCTION=$1
# parse arguments
for ARGUMENT in "$@"
  do
    case $ARGUMENT in
      --coord_env_path=*)
        ENV_DIR=$(echo $ARGUMENT | cut -f2 -d=)
        ;;
      --coord_application=*)
        COORD_APPLICATION=$(echo $ARGUMENT | cut -f2 -d=)
        ;;
      --action=*)
        input=$(echo $ARGUMENT | cut -f2 -d=)
        if [[ $input = "apply" || $input = "destroy" || $input = "plan" ]]
        then
          ACTION=$input
        else
          printf "ERROR: Input action must be one of (apply|destroy|plan).\n"
          exit 1
        fi
        ;;
      --auto_approve=*)
        AUTO_APPROVE=$(echo $ARGUMENT | cut -f2 -d=)
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

if [[ -z ${ENV_DIR+x} ]]; then
  printf "ERROR: Coordinator environment path (COORD_ENV_PATH) must be provided.\n"
  exit 1
fi

if [[ -z ${COORD_APPLICATION+x} ]]; then
  printf "ERROR: Coordinator application (COORD_APPLICATION) must be provided.\n"
  exit 1
fi

deploy-coordinator-terraform
