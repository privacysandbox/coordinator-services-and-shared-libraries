#!/bin/bash
# Copyright 2025 Google LLC
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

# Runs tflint in coordinator Terraform directories.

set -e

REPO_ROOT="$(git rev-parse --show-toplevel)"

bazel run @tflint//:tflint -- --init
TFLINT_OPA_POLICY_DIR="${REPO_ROOT}/.tflint.d/policies" \
  bazel run @tflint//:tflint -- \
  --config "${REPO_ROOT}/.tflint.hcl" \
  --chdir "${REPO_ROOT}/coordinator/terraform/gcp" \
  --recursive
