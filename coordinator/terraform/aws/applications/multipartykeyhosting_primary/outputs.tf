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

output "publickey_primary_api_gateway_url" {
  value = module.publickeyhostingservice_primary.api_gateway_url
}

output "publickey_secondary_api_gateway_url" {
  value = module.publickeyhostingservice_secondary.api_gateway_url
}

output "encryptionkey_api_gateway_url" {
  value = module.apigateway.api_gateway_url
}

output "encryptionkey_api_gateway_endpoint_arn" {
  value = format("%s/*/GET/%s/encryptionKeys/*", module.apigateway.api_gateway_execution_arn, var.api_version)
}

output "cloudfront_domain" {
  value = module.publickeyhostingcloudfront.cloudfront_domain
}

output "kms_key_arn" {
  value = module.split_key_generation_service.kms_key_arn
}

output "encryption_key_signature_key_arn" {
  value = module.split_key_generation_service.encryption_key_signature_key_arn
}

output "public_key_api" {
  value = format("https://%s/%s/publicKeys", (var.enable_domain_management ? module.publickeyhostingcloudfront.public_key_domain : module.publickeyhostingcloudfront.cloudfront_domain), var.api_version)
}

output "encryptionkeyservice_base_url" {
  value = format("%s/%s", (var.enable_domain_management ? "https://${module.privatekeydomainprovider[0].private_key_domain}" : module.apigateway.api_gateway_url), var.api_version)
}
