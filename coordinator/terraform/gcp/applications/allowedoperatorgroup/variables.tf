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

variable "owners" {
  description = "Google group owners."
  type        = list(string)
  default     = []
}

variable "managers" {
  description = "Google group managers."
  type        = list(string)
  default     = []
}

variable "members" {
  description = "Google group members."
  type        = list(string)
  default     = []
}

variable "organization_domain" {
  description = "Domain of the organization to create the Google group in (e.g. example.com)."
  type        = string
}

variable "group_name_prefix" {
  description = "Google group name prefix (Should be unique per organization and valid email local-part format)."
  type        = string
}

variable "initial_group_config" {
  description = "Initial configuration options for creating a Group"
  type        = string
  default     = "EMPTY"
}

variable "customer_id" {
  description = "Customer ID of the organization to create the group in"
  type        = string
  default     = ""
}
