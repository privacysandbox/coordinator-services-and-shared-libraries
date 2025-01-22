# Copyright 2024 Google LLC
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

output "workload_account_id_to_config" {
  description = "Map of operator AWS account IDs to variables used by the operator for federation and attestation"
  value = {
    for aws_account_id, roles in var.aws_account_id_to_role_names :
    aws_account_id => {
      aws_assumed_role_arn                     = module.aws_roleprovider.workload_to_assumed_role_map[aws_account_id]
      gcp_workload_identity_pool_name          = module.federated_workload_identity_pool_provider.aws_account_to_params[aws_account_id].workload_identity_pool_name
      gcp_workload_identity_pool_provider_name = module.federated_workload_identity_pool_provider.aws_account_to_params[aws_account_id].workload_identity_pool_provider_name
      gcp_federated_service_account            = module.federated_workload_identity_pool_provider.aws_account_to_params[aws_account_id].wip_federated_service_account
    }
  }
}

output "aws_kms_key_encryption_key_arn" {
  value = module.aws_key_encryption_key.aws_kms_key_encryption_key_arn
}

output "aws_kms_key_encryption_key_role_arn" {
  value = module.aws_key_encryption_key.aws_kms_key_encryption_key_role_arn
}
