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

output "public_key_service_loadbalancer_ip" {
  value = module.multipartykeyhosting_primary.public_key_service_loadbalancer_ip
}

output "public_key_base_url" {
  value = module.multipartykeyhosting_primary.public_key_base_url
}

output "encryption_key_service_loadbalancer_ip" {
  value = module.multipartykeyhosting_primary.encryption_key_service_loadbalancer_ip
}

output "encryption_key_base_url" {
  value = module.multipartykeyhosting_primary.encryption_key_base_url
}

output "key_encryption_key_id" {
  value = module.multipartykeyhosting_primary.key_encryption_key_id
}

output "key_generation_service_account" {
  value = module.multipartykeyhosting_primary.key_generation_service_account
}

output "key_generation_service_account_unique_id" {
  value = module.multipartykeyhosting_primary.key_generation_service_account_unique_id
}

output "dns_authorization_cname_records_for_alternative_domains" {
  value = module.multipartykeyhosting_primary.dns_authorization_cname_records_for_alternative_domains
}

