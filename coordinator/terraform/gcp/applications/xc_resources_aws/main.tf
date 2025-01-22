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

module "federated_workload_identity_pool_provider" {
  source = "../../modules/federated_workloadidentitypoolprovider"

  project_id                                      = var.wip_project_id_override == null ? var.project_id : var.wip_project_id_override
  environment                                     = var.environment
  aws_account_id_to_role_names                    = var.aws_account_id_to_role_names
  wip_allowed_service_account_project_id_override = var.wip_allowed_service_account_project_id_override == null ? var.project_id : var.wip_allowed_service_account_project_id_override
  enable_audit_log                                = var.enable_audit_log
}

module "aws_key_encryption_key" {
  source = "../../modules/aws_keyencryptionkey"

  environment                             = var.environment
  key_encryptor_service_account_unique_id = var.key_encryptor_service_account_unique_id
}

module "aws_roleprovider" {
  source = "../../modules/aws_roleprovider"

  environment                   = var.environment
  aws_key_encryption_key_arn    = module.aws_key_encryption_key.aws_kms_key_encryption_key_arn
  aws_account_id_to_role_names  = var.aws_account_id_to_role_names
  aws_attestation_enabled       = var.aws_attestation_enabled
  aws_attestation_pcr_allowlist = var.aws_attestation_pcr_allowlist
}
