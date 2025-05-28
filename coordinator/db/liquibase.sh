#!/bin/bash
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

# Wrapper for running Liquibase on KeyDB.
set -euo pipefail

SCRIPT_PATH="$(realpath "$0")"
SCRIPT_DIR="$(dirname "${SCRIPT_PATH}")"

main() {
  # Substitution for redacting OAuth token from JDBC URL in logs.
  # It looks for "oauthToken=" and replaces its value until a "&", ";", or whitespace.
  local readonly SED_OAUTH_REDACTION="s/(oauthToken=)([^;&[:space:]]+)/\1[REDACTED]/g"

  if [[ -z "${PROJECT:-}" ]]; then
    echo "\$PROJECT must be specified" >&2
    exit 1
  elif [[ -z "${INSTANCE:-}" ]]; then
    echo "\$INSTANCE must be specified" >&2
    exit 1
  elif [[ -z "${DATABASE:-}" ]]; then
    echo "\$DATABASE must be specified" >&2
    exit 1
  fi

  local jdbc_url="jdbc:cloudspanner:/projects/${PROJECT}/instances/${INSTANCE}/databases/${DATABASE}"

  if [[ -n "${GOOGLE_OAUTH_ACCESS_TOKEN:-}" ]]; then
    echo "GOOGLE_OAUTH_ACCESS_TOKEN is set. Using it for authentication."
    jdbc_url+=";oauthToken=${GOOGLE_OAUTH_ACCESS_TOKEN}"
  fi
\
  cd "${SCRIPT_DIR}"

  LIQUIBASE_COMMAND_URL="${jdbc_url}" bazel run //java/external:liquibase -- \
    --search-path "${SCRIPT_DIR}" \
    --changelog-file changelog.cloudspanner.yaml \
    "$@" \
    > >(sed -E "${SED_OAUTH_REDACTION}") \
    2> >(sed -E "${SED_OAUTH_REDACTION}" >&2)
}

main "$@"
