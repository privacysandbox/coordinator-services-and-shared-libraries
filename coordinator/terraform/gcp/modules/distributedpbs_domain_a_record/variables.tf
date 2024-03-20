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
  description = "Description for the environment. Used to prefix resources. e.g. dev, staging, production."
  type        = string
  nullable    = false
}

variable "parent_domain_name" {
  description = "The parent domain name used for this PBS environment. NOTE: The hosted zone must exist."
  type        = string
  nullable    = false
}

variable "parent_domain_project" {
  description = "The project owning the parent domain name used for this PBS environment. NOTE: The hosted zone must exist."
  type        = string
  nullable    = false
}

variable "pbs_domain" {
  description = "The PBS domain."
  type        = string
  nullable    = false
}

variable "pbs_ip_address" {
  description = "The PBS IP address"
  type        = string
  nullable    = false
}
