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

module "workload_identity_pool" {
  source                                          = "../../modules/workloadidentitypoolprovider"
  project_id                                      = var.wipp_project_id_override == null ? var.project_id : var.wipp_project_id_override
  wip_allowed_service_account_project_id_override = var.wip_allowed_service_account_project_id_override == null ? var.project_id : var.wip_allowed_service_account_project_id_override
  environment                                     = var.environment

  # Limited to 32 characters
  workload_identity_pool_id          = "${var.environment}-opwip"
  workload_identity_pool_description = "Workload Identity Pool to manage KEK access for operators using attestation ."

  # Limited to 32 characters
  workload_identity_pool_provider_id = "${var.environment}-opwip-pvdr"
  wip_provider_description           = "WIP Provider to manage OIDC tokens and attestation"

  # Limited to 30 characters
  wip_verified_service_account_id           = "${var.environment}-opverifiedusr"
  wip_verified_service_account_display_name = "${var.environment} Verified Operator User"

  # Limited to 30 characters
  wip_allowed_service_account_id           = "${var.environment}-opallowedusr"
  wip_allowed_service_account_display_name = "${var.environment} Allowed Operator User"

  key_encryption_key_id                        = var.key_encryption_key_id
  allowed_wip_iam_principals                   = var.allowed_wip_iam_principals
  allowed_wip_user_group                       = var.allowed_wip_user_group
  enable_attestation                           = var.enable_attestation
  assertion_tee_swname                         = var.assertion_tee_swname
  assertion_tee_support_attributes             = var.assertion_tee_support_attributes
  assertion_tee_container_image_reference_list = var.assertion_tee_container_image_reference_list
  assertion_tee_container_image_hash_list      = var.assertion_tee_container_image_hash_list

  enable_audit_log = var.enable_audit_log
}

# Allow Verified Service Account to decrypt with given KEK
resource "google_kms_crypto_key_iam_member" "workload_identity_member" {
  crypto_key_id = var.key_encryption_key_id
  role          = "roles/cloudkms.cryptoKeyDecrypter"
  member        = "serviceAccount:${module.workload_identity_pool.wip_verified_service_account}"
  depends_on    = [module.workload_identity_pool]
}
