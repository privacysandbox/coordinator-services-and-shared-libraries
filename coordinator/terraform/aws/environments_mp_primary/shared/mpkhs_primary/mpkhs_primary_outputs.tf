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

output "public_key_urls" {
  description = "Endpoint URLs for fetching public encryption keys. To be passed to clients."
  value = {
    public_keys_cdn_url = module.multipartykeyhosting_primary.public_key_api
    public_keys_internal_base_urls = {
      primary  = module.multipartykeyhosting_primary.publickey_primary_api_gateway_url
      failover = module.multipartykeyhosting_primary.publickey_secondary_api_gateway_url
    }
  }
}

output "roleprovider_variables" {
  description = "Variables which must be input to this deployment's corresponding 'roleprovider' environment."
  value = {
    private_key_api_gateway_arn = module.multipartykeyhosting_primary.encryptionkey_api_gateway_endpoint_arn
    private_key_encryptor_arn   = module.multipartykeyhosting_primary.kms_key_arn
  }
}

output "worker_variables" {
  description = "Variables which must be shared with the operator/worker environments configured to use this deployment."
  value = {
    "--coordinator_a_encryption_key_service_base_url" = module.multipartykeyhosting_primary.encryptionkeyservice_base_url
  }
}

output "encryption_key_signature_key_arn" {
  description = "Key associated with this coordinator used to sign public key material."
  value       = module.multipartykeyhosting_primary.encryption_key_signature_key_arn
}
