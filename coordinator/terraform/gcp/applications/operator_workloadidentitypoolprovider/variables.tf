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

variable "project_id" {
  description = "GCP Project ID in which this module will be created."
  type        = string
}

variable "wipp_project_id_override" {
  description = "Override the GCP Project ID in which workload identity pool (wip) and workload identity pool provider (wipp) will be created in. The value of `project_id` is used by default."
  type        = string
  default     = null
}

variable "wip_allowed_service_account_project_id_override" {
  description = "Override the GCP Project ID in which WIP allowed service account will be created in. The value of `project_id` is used by default."
  type        = string
  default     = null
}

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production."
  type        = string
}

variable "key_encryption_key_id" {
  description = "KMS key which will be used to decrypt."
  type        = string
}

variable "allowed_wip_iam_principals" {
  description = "List of allowed IAM principals."
  type        = list(string)
  default     = []
}

variable "allowed_wip_user_group" {
  description = "Google Group to manage allowed users. Deprecated - use allowed_wip_iam_principals instead."
  type        = string
  default     = null
}

variable "enable_attestation" {
  description = "Enable attestation requirement for this Workload Identity Pool. Assertion TEE vars required."
  type        = bool
  default     = true
}

variable "assertion_tee_swname" {
  description = "Software Running TEE. Example: 'CONFIDENTIAL_SPACE'."
  type        = string
  default     = "CONFIDENTIAL_SPACE"
}

variable "assertion_tee_support_attributes" {
  description = "Required support attributes of the Confidential Space image. Example: \"STABLE\", \"LATEST\". Empty for debug images."
  type        = list(string)
  default     = ["STABLE"]
}

variable "assertion_tee_container_image_reference_list" {
  description = "List of acceptable image names TEE can run."
  type        = list(string)
  default     = []
}

variable "assertion_tee_container_image_hash_list" {
  description = "List of acceptable binary hashes TEE can run."
  type        = list(string)
  default     = []
}

variable "enable_audit_log" {
  description = "Enable Data Access Audit Logging for projects containing the WIP and Service Accounts. (If both Type 1 and Type 2 Coordinators are using the same projects, this only needs to be enabled on one of them.)"
  type        = bool
  default     = false
}
