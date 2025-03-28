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

output "coordinator_a_mpkhs_primary_variables" {
  description = "Variables which must be shared with Coordinator A as input parameters for mpkhs_variables."
  value = {
    coordinator_b_assume_role_arn = module.multipartykeyhosting_secondary.peer_coordinator_assume_role_arn
    keystorage_api_base_url       = module.multipartykeyhosting_secondary.keystorageservice_base_url
  }
}

output "roleprovider_variables" {
  description = "Variables which must be input to this deployment's corresponding 'roleprovider' environment."
  value = {
    private_key_api_gateway_arn = module.multipartykeyhosting_secondary.encryptionkey_api_gateway_endpoint_arn
    private_key_encryptor_arn   = module.multipartykeyhosting_secondary.kms_key_arn
  }
}

output "worker_variables" {
  description = "Variables which must be shared with the operator/worker environments configured to use this deployment."
  value = {
    "--coordinator_b_encryption_key_service_base_url" = module.multipartykeyhosting_secondary.encryptionkeyservice_base_url
  }
}

output "encryption_key_signature_key_arn" {
  description = "Key associated with this coordinator used to sign public key material."
  value       = module.multipartykeyhosting_secondary.encryption_key_signature_key_arn
}

output "key_sync_assume_role_arn" {
  description = "ARN of the AWS role to be assumed for cross-cloud key sync."
  value       = module.multipartykeyhosting_secondary.key_sync_assume_role_arn
}

output "key_sync_kms_key_uri" {
  description = "URI of the AWS KMS key used to wrap legacy AWS private keys."
  value       = module.multipartykeyhosting_secondary.kms_key_arn
}

output "key_sync_keydb_region" {
  description = "Region of the Dynamodb KeyDB table for key sync writes."
  value       = module.multipartykeyhosting_secondary.key_sync_keydb_region
}

output "key_sync_keydb_table_name" {
  description = "Table name of the Dynamodb KeyDB for key sync writes."
  value       = module.multipartykeyhosting_secondary.key_sync_keydb_table_name
}
