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

variable "remote_coordinator_aws_account_id" {
  description = "The account ID of the remote coordinator."
  type        = string
  nullable    = false
}

variable "pbs_auth_api_gateway_arn" {
  description = "The ARN of the PBS auth API gateway."
  type        = string
  nullable    = false
}

variable "pbs_auth_table_name" {
  description = "The name of the reporting origin table."
  type        = string
  nullable    = false
}

variable "pbs_auth_table_v2_name" {
  description = "DynamoDB table name of distributed pbs auth table V2."
  type        = string
  nullable    = false
}

variable "remote_environment" {
  description = "The name of the remote coordinator's environment used as a role prefix."
  type        = string
  nullable    = false
}
