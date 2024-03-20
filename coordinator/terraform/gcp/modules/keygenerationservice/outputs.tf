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

output "key_encryption_key_id" {
  value = google_kms_crypto_key.key_encryption_key.id
}

output "subscription_id" {
  value = google_pubsub_subscription.key_generation_pubsub_subscription.name
}

output "key_generation_service_account" {
  value = google_service_account.key_generation_service_account.email
}