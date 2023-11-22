# Copyright 2023 Google LLC
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
  description = "environment where this service is deployed. Eg: dev/prod, etc"
  type        = string
}

variable "region" {
  description = "Region where all services will be created."
  type        = string
}

variable "account_id" {
  description = "Current account ID"
  type        = string
}

variable "privacy_budget_api_gateway_id" {
  description = "The API Gateway ID of the privacy budget service monitored by this dashboard."
  type        = string
}

variable "privacy_budget_dashboard_time_period_seconds" {
  description = "Time period that acts as a window for dashboard metrics. Measured in seconds."
  type        = number
}

variable "privacy_budget_load_balancer_id" {
  description = "PBS loadbalancer identifier."
  type        = string
}

variable "privacy_budget_autoscaling_group_name" {
  description = "Logical grouping of PBS EC2 instances."
  type        = string
}

variable "custom_alarm_label" {
  description = "Add any string to the label to help filtering, allowed chars (a-zA-Z_-) max 30 chars"
  type        = string
}
