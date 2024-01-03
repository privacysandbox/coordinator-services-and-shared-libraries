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

provider "aws" {
  region = var.aws_region
}

locals {
  privacy_budget_auth_table_v2_name = "${var.environment}-${var.privacy_budget_auth_table_v2_name_suffix}"
}

module "roleprovider" {
  source = "../../../applications/roleprovider"

  environment                       = var.environment
  private_key_encryptor_arn         = var.private_key_encryptor_arn
  private_key_api_gateway_arn       = var.private_key_api_gateway_arn
  privacy_budget_api_gateway_arn    = var.privacy_budget_api_gateway_arn
  privacy_budget_auth_table_name    = var.privacy_budget_auth_table_name
  attestation_condition_keys        = var.attestation_condition_keys
  allowed_principals_map_v2         = var.allowed_principals_map_v2
  privacy_budget_auth_table_v2_name = local.privacy_budget_auth_table_v2_name
}
