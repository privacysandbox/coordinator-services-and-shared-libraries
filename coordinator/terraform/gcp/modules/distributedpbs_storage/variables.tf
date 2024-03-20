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
  type     = string
  nullable = false
}

variable "region" {
  description = "Region where all resources will be created."
  type        = string
  nullable    = false
}

variable "pbs_cloud_storage_journal_bucket_force_destroy" {
  description = "Whether to force destroy the bucket even if it is not empty."
  type        = bool
  nullable    = false
}

variable "pbs_cloud_storage_journal_bucket_versioning" {
  description = "Enable bucket versioning."
  type        = bool
  nullable    = false
}

# Similar to Point-in-time recovery for AWS DynamoDB
# Must be between 1 hour and 7 days. Can be specified in days, hours, minutes, or seconds.
# eg: 1d, 24h, 1440m, and 86400s are equivalent.
variable "pbs_spanner_database_retention_period" {
  description = "Duration to maintain table versioning for point-in-time recovery."
  type        = string
  nullable    = false
}

variable "pbs_spanner_instance_processing_units" {
  description = "Spanner's compute capacity. 1000 processing units = 1 node and must be set as a multiple of 100."
  type        = number
  nullable    = false
}

variable "pbs_spanner_database_deletion_protection" {
  description = "Prevents destruction of a table when it is not empty."
  type        = bool
  nullable    = false
}
