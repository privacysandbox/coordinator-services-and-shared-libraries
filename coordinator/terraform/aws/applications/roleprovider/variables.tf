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

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production."
  type        = string
}

variable "allowed_principals_map_v2" {
  type        = map(list(string))
  description = "Map of AWS account IDs, that a coordinator_assume_role will be created for, to the list of their associated (eTLD+1) sites. If PBS is deployed and used, allowed_principals_map_v2 should be specified instead of allowed_principals_set."
}

variable "allowed_principals_set" {
  type        = set(string)
  description = "Set of AWS account IDs, that a coordinator_assume_role will be created for."
}

variable "private_key_encryptor_arn" {
  type        = string
  description = "KMS key which will be used to decrypt."
}

variable "private_key_api_gateway_arn" {
  type        = string
  description = "API Gateway used to access the private key service. If unspecified the generated role will not be granted access."
}

variable "privacy_budget_api_gateway_arn" {
  type        = string
  description = "API Gateway used to access the privacy budget service. If unspecified the generated role will not be granted access."
  nullable    = true
}

variable "privacy_budget_auth_table_v2_name" {
  description = "DynamoDB table name of distributed pbs auth table V2"
  type        = string
  nullable    = true
}

variable "attestation_condition_keys" {
  description = <<-EOT
    Map of Condition Keys for Nitro Enclave attestation. Key = Condition key type (PCR0). Value = list of acceptable enclave hashes.
    If map is empty, then no condition is applied and any enclave can decrypt with the assume_role.
    EOT
  type        = map(list(string))
  validation {
    condition     = (length([for l in var.attestation_condition_keys : [for pcr in l : 1]]) < 55)
    error_message = "Only 54 elements supported in the attestation_condition_keys list"

  }
}

variable "attestation_pcr_allowlist" {
  description = <<-EOT
    List of PCR0s to allowlist for Nitro Enclave attestation.
    If list is empty, then no condition is applied and any enclave can decrypt with the assume_role.
    EOT
  type        = list(string)
}
