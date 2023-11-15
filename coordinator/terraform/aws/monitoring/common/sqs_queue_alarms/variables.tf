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

variable "sns_topic_arn" {
  description = "SNS topic ARN to forward alerts to"
  type        = string
}

variable "sqs_queue_name" {
  description = "Name of the SQS Queue -- used for creating alarms."
  type        = string
}

variable "alarm_label_sqs_queue" {
  description = "Name of the SQS Queue -- used for naming alarms."
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

variable "total_queue_messages_high_threshold" {
  description = "Number of messages in the SQS queue considered high."
  type        = string
}
