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

output "get_public_key_loadbalancer_ip" {
  value = module.publickeyhostingservice.get_public_key_loadbalancer_ip
}

output "public_key_base_url" {
  value = var.enable_domain_management ? "https://${local.public_key_domain}" : "http://${module.publickeyhostingservice.get_public_key_loadbalancer_ip}"
}

output "encryption_key_service_cloudfunction_url" {
  value = module.encryptionkeyservice.encryption_key_service_cloudfunction_url
}

output "encryption_key_service_loadbalancer_ip" {
  value = module.encryptionkeyservice.encryption_key_service_loadbalancer_ip
}

output "encryption_key_base_url" {
  value = var.enable_domain_management ? "https://${local.encryption_key_domain}" : "http://${module.encryptionkeyservice.encryption_key_service_loadbalancer_ip}"
}

output "key_encryption_key_id" {
  value = module.keygenerationservice.key_encryption_key_id
}

output "key_generation_service_account" {
  value = module.keygenerationservice.key_generation_service_account
}
