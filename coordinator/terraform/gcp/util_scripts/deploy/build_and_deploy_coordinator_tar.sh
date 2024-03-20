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
  echo "Usage:  ./build_and_deploy_coordinator_tar.sh --coord_env=<environment> --coord_application=<application> [--action=<action>] [--auto_approve=<true/false>]"
  printf "\n\t--coord_env         - The coordinator environment to deploy e.g. environments_mp_primary/demo.\n"
  printf "\t--coord_application - The coordinator application to deploy e.g. operator_wipp.\n"
  printf "\t--action            - Terraform action to take (apply|destroy|plan). Default is apply.\n"
  printf "\t--auto_approve      - Whether to auto-approve the Terraform action. Default is false.\n"
}

setup() {
  # Create temporary directory for coordinator
    COORD_TEMP="/tmp/$COORD_ENV/$COORD_APPLICATION/coord_tar"
    rm -rf "$COORD_TEMP"
    mkdir -p "$COORD_TEMP"

    # build tar
    bazel build //coordinator/terraform/gcp:multiparty_coordinator_tar

    # extract tar
    TAR_FILE="$(bazel info bazel-bin)/coordinator/terraform/gcp/multiparty_coordinator_tar.tgz"
    tar xzf "$TAR_FILE" -C "$COORD_TEMP"

    cp -r $(bazel info workspace)/coordinator/terraform/gcp/$COORD_ENV/ "$COORD_TEMP/$COORD_ENV"

    COORD_ENV_PATH="$COORD_TEMP/$COORD_ENV"
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
      --coord_env=*)
        COORD_ENV=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      --coord_application=*)
        COORD_APPLICATION=$(echo "$ARGUMENT" | cut -f2 -d=)
        ;;
      --action=*)
        input=$(echo "$ARGUMENT" | cut -f2 -d=)
        if [[ $input = "apply" || $input = "destroy" || $input = "plan" ]]
        then
          ACTION=$input
        else
          printf "ERROR: Input action must be one of (apply|destroy|plan).\n"
          exit 1
        fi
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

if [[ -z ${COORD_ENV+x} ]]; then
  printf "ERROR: Coordinator environment (coord_env) must be provided.\n"
  exit 1
fi

if [[ -z ${COORD_APPLICATION+x} ]]; then
  printf "ERROR: Coordinator application (coord_application) must be provided.\n"
  exit 1
fi

# Build and extract coordinator tar and environment.
setup

PARAMETERS=''

if [[ -n ${ACTION+x} ]]; then
    PARAMETERS="${PARAMETERS} --action=$ACTION "
fi

if [[ -n ${AUTO_APPROVE+x} ]]; then
    PARAMETERS="${PARAMETERS} --auto_approve=$AUTO_APPROVE "
fi

# Deploy coordinator environment with specified action.
$(bazel info workspace)/coordinator/terraform/gcp/util_scripts/deploy/coordinator_deploy.sh --coord_env_path=$COORD_ENV_PATH --coord_application=$COORD_APPLICATION $PARAMETERS

# deletes the temp directory
function cleanup {
  if [[ -n "${COORD_TEMP+x}" ]]; then
    rm -rf "COORD_TEMP"
  fi
}

# register the cleanup function to be called on the EXIT signal
trap cleanup EXIT
