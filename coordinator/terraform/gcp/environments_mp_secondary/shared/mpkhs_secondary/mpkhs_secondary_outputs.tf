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

output "encryption_key_service_loadbalancer_ip" {
  value = module.multipartykeyhosting_secondary.encryption_key_service_loadbalancer_ip
}

output "encryption_key_base_url" {
  value = module.multipartykeyhosting_secondary.encryption_key_base_url
}

output "key_storage_service_loadbalancer_ip" {
  value = module.multipartykeyhosting_secondary.key_storage_service_loadbalancer_ip
}

output "key_storage_base_url" {
  value = module.multipartykeyhosting_secondary.key_storage_base_url
}

output "key_encryption_key_id" {
  value = module.multipartykeyhosting_secondary.key_encryption_key_id
}

output "workload_identity_pool_provider_name" {
  value = module.multipartykeyhosting_secondary.workload_identity_pool_provider_name
}

output "wip_allowed_service_account" {
  value = module.multipartykeyhosting_secondary.wip_allowed_service_account
}

output "wip_verified_service_account" {
  value = module.multipartykeyhosting_secondary.wip_verified_service_account
}

output "key_storage_service_account_unique_id" {
  value = module.multipartykeyhosting_secondary.key_storage_service_account_unique_id
}
