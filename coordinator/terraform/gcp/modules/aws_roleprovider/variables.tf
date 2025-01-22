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

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production."
  type        = string
}

variable "aws_account_id_to_role_names" {
  description = <<EOT
    A map of AWS account IDs to a list of roles within that account.
    Only the roles in the list will be given access to private key endpoint and attestation.
    If list of roles is empty the whole account is given access.
  EOT
  type        = map(set(string))
}

variable "aws_key_encryption_key_arn" {
  description = "ARN of AWS KMS key encryption key which created roles will decrypt with."
  type        = string
}

variable "aws_attestation_enabled" {
  description = "Enable attestation for the created roles. Should be disabled only for development purposes."
  type        = bool
}

variable "aws_attestation_pcr_allowlist" {
  description = "List of PCR0s to allowlist for Nitro Enclave attestation. Ignored if attestation is disabled."
  type        = set(string)
}
