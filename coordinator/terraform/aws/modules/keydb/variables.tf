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
  description = "Description for the environment, e.g. dev, staging, production"
  type        = string
}

variable "table_name" {
  description = "Name of the DynamoDB table, should include environment"
  type        = string
}

variable "write_capacity" {
  description = "Provisioned write capacity units. Example: 5"
  type        = number
}

variable "read_capacity" {
  description = "Provisioned read capacity units. Example: 5"
  type        = number
}

variable "autoscaling_read_max_capacity" {
  description = "Maximum read capacity units. Example: 500"
  type        = number
}

variable "autoscaling_read_target_percentage" {
  description = "Read capacity target utilization. Example: 70"
  type        = number
}

variable "autoscaling_write_max_capacity" {
  description = "Maximum write capacity units. Example: 500"
  type        = number
}

variable "autoscaling_write_target_percentage" {
  description = "Write capacity target utilization. Example: 70"
  type        = number
}

variable "enable_dynamo_point_in_time_recovery" {
  description = "Allows DynamoDB table data to be recovered after table deletion"
  type        = bool
}

variable "enable_keydb_ttl" {
  description = "Allows DynamoDB table data to delete keys automatically based on the TtlTime attribute."
  type        = bool
}

variable "keydb_replica_regions" {
  description = "Regions where keydb table will be replicated"
  type        = list(string)
}
