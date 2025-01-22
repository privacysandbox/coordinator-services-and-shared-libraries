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

########################
# DO NOT EDIT MANUALLY #
########################

# This file is meant to be shared across all environments (either copied or
# symlinked). In order to make the upgrade process easier, this file should not
# be modified for environment-specific customization.

module "xc_resources_aws" {
  source = "../../../applications/xc_resources_aws"

  environment                             = var.environment
  project_id                              = var.project_id
  aws_account_id_to_role_names            = var.aws_account_id_to_role_names
  key_encryptor_service_account_unique_id = var.key_storage_service_account_unique_id
  aws_attestation_enabled                 = var.aws_attestation_enabled
  aws_attestation_pcr_allowlist           = var.aws_attestation_pcr_allowlist

  enable_audit_log                                = var.enable_audit_log
  wip_project_id_override                         = var.wip_project_id_override
  wip_allowed_service_account_project_id_override = var.wip_allowed_service_account_project_id_override
}
