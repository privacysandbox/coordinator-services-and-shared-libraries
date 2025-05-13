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
set -eu -o pipefail

SCRIPT_PATH="$(realpath "$0")"
SCRIPT_DIR="$(dirname "${SCRIPT_PATH}")"

main() {
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

  cd "${SCRIPT_DIR}"
  exec bazel run //java/external:liquibase -- \
    --search-path "${SCRIPT_DIR}" \
    --changelog-file changelog.cloudspanner.yaml \
    --url "jdbc:cloudspanner:/projects/${PROJECT}/instances/${INSTANCE}/databases/${DATABASE}" \
    "$@"
}

main "$@"
