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

#TODO(b/240608110) Switch to environment
variable "environment_prefix" {
  description = "Value used to prefix the name of the created resources."
  type        = string
  nullable    = false
}

variable "auth_table_read_initial_capacity" {
  type     = number
  default  = 100
  nullable = false
}

variable "auth_table_read_max_capacity" {
  type     = number
  default  = 2500
  nullable = false
}

variable "auth_table_read_scale_utilization" {
  type     = number
  default  = 70
  nullable = false
}

variable "auth_table_write_initial_capacity" {
  type     = number
  default  = 100
  nullable = false
}

variable "auth_table_write_max_capacity" {
  type     = number
  default  = 2500
  nullable = false
}

variable "auth_table_write_scale_utilization" {
  type     = number
  default  = 70
  nullable = false
}

variable "auth_table_enable_point_in_time_recovery" {
  type = bool
}
