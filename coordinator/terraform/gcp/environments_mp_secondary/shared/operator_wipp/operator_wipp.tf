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

########################
# DO NOT EDIT MANUALLY #
########################

# This file is meant to be shared across all environments (either copied or
# symlinked). In order to make the upgrade process easier, this file should not
# be modified for environment-specific customization.

module "operator_workloadidentitypoolprovider" {
  source                                          = "../../../applications/operator_workloadidentitypoolprovider"
  environment                                     = var.environment
  project_id                                      = var.project_id
  wipp_project_id_override                        = var.wipp_project_id_override
  wip_allowed_service_account_project_id_override = var.wip_allowed_service_account_project_id_override
  key_encryption_key_id                           = var.key_encryption_key_id
  allowed_wip_iam_principals                      = var.allowed_wip_iam_principals
  allowed_wip_user_group                          = var.allowed_wip_user_group
  enable_attestation                              = var.enable_attestation
  assertion_tee_swname                            = var.assertion_tee_swname
  assertion_tee_support_attributes                = var.assertion_tee_support_attributes
  assertion_tee_container_image_reference_list    = var.assertion_tee_container_image_reference_list
  assertion_tee_container_image_hash_list         = var.assertion_tee_container_image_hash_list
  enable_audit_log                                = var.enable_audit_log
}
