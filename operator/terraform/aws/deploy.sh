#!/usr/bin/env bash
# Copyright 2023 Google LLC
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
# Deploys operator environment
#
# Args:
#   cmd - "clean" to clean up any previous directories and untracked files
#
# `./deploy.sh help` for usage instructions.

set -euo pipefail

EXTRACT_PATHS=("./applications" "./modules" "./environments")
REQUIRE_CONFIRM=1
ENVIRONMENT_NAME="mp-release"
OUTPUT_VARIABLE=""
SILENT=false

# Prints usage instructions
print_usage() {
  echo "Usage: ./deploy.sh (clean|setup)"
  printf "\tclean - Removes all locally untracked files.\n"
  printf "\tdeploy-mp-tar-release-operator <ENVIRONMENT> - Performs setup and deploys the specified operator environment.\n"
  printf "\tget-mp-tar-release-operator-output <ENVIRONMENT> <VARIABLE> - Gets the raw operator output <VARIABLE> from terraform.\n"
}

confirm_yes() {
  local prompt=$1
  echo ""
  read -p "$prompt [yes/no]: " yn
  case $yn in
    yes ) return 0;;
    no ) echo "Exiting" && exit;;
    * ) echo "Unknown input, exiting" && exit;;
  esac
}

cleanup() {
  local files
  # Grep returns 1 if no matches are found. This should not be a fail condition for the script.
  set +e
  files=$(git ls-files --others ${EXTRACT_PATHS[@]} | grep --invert-match $ENVIRONMENT_NAME)
  set -e

  if [[ $files == "" ]]; then
    return 0
  fi

  if [[ $REQUIRE_CONFIRM = 1 ]]; then
    echo "$files"
    confirm_yes "The above local untracked files will be deleted, continue?"
  fi
  rm $files
  rm -f *.tgz
  # Delete any empty directories left behind (folders aren't tracked by git).
  find ${EXTRACT_PATHS[@]} -type d -empty -delete
}

deploy-mp-tar-release-operator() {
  set -e
  REQUIRE_CONFIRM=0
  cleanup

  ENV_DIR="environments/$ENVIRONMENT_NAME/"
  bazel run @terraform//:terraform -- -chdir=$ENV_DIR init -input=false
  bazel run @terraform//:terraform -- -chdir=$ENV_DIR apply -input=false -auto-approve \
    || echo "Failed `terraform apply` on the operator environment at $ENV_DIR." && exit 1
}

get-mp-tar-release-operator-output() {
  ENV_DIR="environments/$ENVIRONMENT_NAME/"
  bazel run @terraform//:terraform -- -chdir=$ENV_DIR output -raw $OUTPUT_VARIABLE
}

# Print usage instructions if no command was provided.
if [[ $# == 0 ]]; then
  print_usage
  exit 1
fi

# Parse the function, environment, and output variable if provided.
# Sets script to silent when retrieving terraform output.
FUNCTION_CALL="$1"
if [[ $FUNCTION_CALL = "get-mp-tar-release-operator-output" ]]; then
  SILENT=true
  ENVIRONMENT_NAME=$2
  OUTPUT_VARIABLE=$3
elif [[ $# -ge 2 ]]; then
  ENVIRONMENT_NAME="$2"
  echo "Environment override: $ENVIRONMENT_NAME"
fi

case $FUNCTION_CALL in
  clean )
    cleanup
    ;;
  deploy-mp-tar-release-operator )
    deploy-mp-tar-release-operator
    ;;
  get-mp-tar-release-operator-output )
    get-mp-tar-release-operator-output
    ;;
  help )
    print_usage
    ;;
  * )
    print_usage
    echo "Unknown command '$FUNCTION_CALL', exiting"
esac
