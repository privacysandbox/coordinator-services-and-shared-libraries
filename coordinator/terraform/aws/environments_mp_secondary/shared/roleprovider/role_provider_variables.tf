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

variable "aws_region" {
  description = "AWS region where to deploy resources."
  type        = string
}

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production."
  type        = string

  validation {
    condition     = length(var.environment) <= 18
    error_message = "The max length of the environment name is 18 characters."
  }
}

variable "allowed_principals_map_v2" {
  description = "Map of AWS account IDs, that will assume the coordinator_assume_role, to the list of their associated (eTLD+1) sites."
  type        = map(list(string))
  default     = {}
}

variable "private_key_encryptor_arn" {
  description = "KMS key which will be used to decrypt."
  type        = string
}

variable "private_key_api_gateway_arn" {
  description = "API Gateway used to access the private key service."
  type        = string
}

variable "privacy_budget_api_gateway_arn" {
  description = "API Gateway used to access the privacy budget service."
  type        = string
}

variable "privacy_budget_auth_table_name" {
  description = "DynamoDB table name of distributed pbs auth table"
  type        = string
  default     = ""
}

variable "privacy_budget_auth_table_v2_name_suffix" {
  description = <<-EOT
    DynamoDB table name suffix of distributed pbs auth table V2.
    This suffix when combined with environment name prefix forms the table name.
    EOT
  type        = string
  default     = "pbs-authorization-v2"
}

variable "attestation_condition_keys" {
  description = <<-EOT
    Map of Condition Keys for Nitro Enclave attestation. Key = Condition key type (PCR0). Value = list of acceptable enclave hashes.
    If map is empty, then no condition is applied and any enclave can decrypt with the assume_role.
    EOT
  type        = map(list(string))
  default     = {}
}
