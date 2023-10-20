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
  description = "environment where this service is deployed. Eg: dev/prod, etc"
  type        = string
}

variable "lambda_function_name" {
  description = "full function_name of the lambda to create alarms for. Used in alarm and metric names."
  type        = string
}

variable "sns_topic_arn" {
  description = "SNS topic ARN to forward alerts to"
  type        = string
}

variable "cloudwatch_log_group_name" {
  description = "Cloudwatch log group name to create alerts for (logs with ERROR in the name)"
  type        = string
}

variable "custom_alarm_label" {
  description = "Add any string to the label to help filtering, allowed chars (a-zA-Z_-) max 30 chars"
  type        = string
}

################################################################################
# Alarm Variables.
################################################################################

variable "eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = string
}

variable "execution_error_threshold" {
  description = "Alarming threshold for Lambda execution errors. Must be in decimal form: 10% = 0.10. Example: '0.0'."
  type        = string
}

variable "error_log_threshold" {
  description = "Alarming threshold for the the count of logs containing ERROR found in the provided cloudwatch_log_group_name. Example: '0'."
  type        = string
}

variable "max_duration_threshold_ms" {
  description = "Lambda max duration in ms to send alarm. Useful for timeouts. Example: '9999'."
  type        = string
}
variable "lambda_function_name_alarm" {
  description = "full function_name of the lambda to create alarm name"
  type        = string
}
