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

output "keystorage_api_gateway_url" {
  value = module.keystorageapigateway.api_gateway_url
}

output "encryptionkey_api_gateway_url" {
  value = module.keyhostingapigateway.api_gateway_url
}

output "encryptionkey_api_gateway_endpoint_arn" {
  value = format("%s/*/GET/%s/encryptionKeys/*", module.keyhostingapigateway.api_gateway_execution_arn, var.api_version)
}

output "kms_key_arn" {
  value = module.keystorageservice.kms_key_arn
}

# TODO: reflect domain management settings
output "keystorageservice_base_url" {
  value = format("%s/%s", module.keystorageapigateway.api_gateway_url, var.api_version)
}

output "encryptionkeyservice_base_url" {
  value = format("%s/%s", (var.enable_domain_management ? "https://${module.privatekeydomainprovider[0].private_key_domain}" : module.keyhostingapigateway.api_gateway_url), var.api_version)
}

output "peer_coordinator_assume_role_arn" {
  value = module.keystorageservice.peer_coordinator_assume_role_arn
}

output "encryption_key_signature_key_arn" {
  value = module.keystorageservice.encryption_key_signature_key_arn
}
