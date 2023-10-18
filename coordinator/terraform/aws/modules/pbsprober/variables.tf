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
  description = "Description for the environment, e.g. dev, staging, production"
  type        = string
}

variable "region" {
  description = "Region where pbs prober will be created."
  type        = string
}

variable "pbsprober_jar" {
  description = "Path to the jar file for lambda function."
  type        = string
}

variable "coordinator_a_privacy_budgeting_endpoint" {
  description = "Coordinator A's HTTP endpoint for privacy budgeting."
  type        = string
}

variable "coordinator_a_privacy_budget_service_auth_endpoint" {
  description = "Coordinator A's Auth endpoint for privacy budgeting service."
  type        = string
}

variable "coordinator_a_role_arn" {
  description = "Role of PBS Prober to call coordinator A."
  type        = string
}

variable "coordinator_a_region" {
  description = "The region of coordinator A services."
  type        = string
}

variable "coordinator_b_privacy_budgeting_endpoint" {
  description = "Coordinator B's HTTP endpoint for privacy budgeting."
  type        = string
}

variable "coordinator_b_privacy_budget_service_auth_endpoint" {
  description = "Coordinator B's Auth endpoint for privacy budgeting service."
  type        = string
}

variable "coordinator_b_role_arn" {
  description = "Role of PBS Prober to call coordinator B."
  type        = string
}

variable "coordinator_b_region" {
  description = "The region of coordinator B services."
  type        = string
}

variable "reporting_origin" {
  description = "The reporting origin of PBS Prober."
  type        = string
}

variable "alarms_enabled" {
  description = "Enable alarms for services."
  type        = bool
  default     = false
}

variable "alarms_notification_email" {
  description = "Email to receive alarms for services."
  type        = string
  default     = ""
}

variable "alarms_sns_topic_arn" {
  description = "SNS topic ARN to forward alerts to"
  type        = string
  default     = ""
}

variable "alarms_sqs_queue_arn" {
  description = "SQS queue ARN to forward alerts to"
  type        = string
  default     = ""
}

variable "custom_alarm_label" {
  description = "Add any string to the label to help filtering, allowed chars (a-zA-Z_-) max 30 chars"
  type        = string
}
