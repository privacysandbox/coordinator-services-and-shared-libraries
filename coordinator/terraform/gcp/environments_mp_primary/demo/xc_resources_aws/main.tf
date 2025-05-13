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

module "xc_resources_aws" {
  source = "../../../applications/xc_resources_aws"

  environment = "<environment_name>"
  project_id  = "<project_id>"

  key_generation_service_account_unique_id = "<output from mpkhs_primary>"

  # Restrict access to specific roles within each operator AWS account.
  # If roles list is empty access is granted to the entire account.
  # Keep in mind that role name (not role ARN) should be used.
  # For example for role ARN arn:aws:iam::123123123123:role/env-a-AggregationServiceWorkerRole
  # <aws_account_id> is "123123123123" and <role_name> is "env-a-AggregationServiceWorkerRole".
  aws_account_id_to_role_names = {
    "<aws_account_id>" : ["<role_name_1>", "<role_name_2>"]
  }

  # Make sure to update the allowlist when enabling attestation, otherwise no
  # workloads will attest successfully.
  aws_attestation_enabled = false
  # Allowlist of PCR0 hashes to be used for attesting Nitro Enclaves.
  # aws_attestation_pcr_allowlist = [
  #  "8588d35e2370588078bf5192908b970e1ff77acf845f317ac2c2cbe1bb9b22d7775e0a022f43f1168fda7fc1b7e8af01"
  # ]

  # wipp_project_id_override = <project_id_to_deploy_wip_wipp> # defaults to `project_id` if not set
  # wip_allowed_service_account_project_id_override = <project_id_to_create_wip_allowed_service_account> # defaults to `project_id` if not set
}
