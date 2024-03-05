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

variable "cloudfront_alarm_name_prefix" {
  description = "Name of the Cloudfront endpoint -- used for naming alarms."
  type        = string
}

variable "custom_alarm_label" {
  description = "Add any string to the label to help filtering, allowed chars (a-zA-Z_-) max 30 chars"
  type        = string
}

variable "sns_topic_arn" {
  description = "ARN for SNS topic in us-east-1 to send alarm notifications"
  type        = string
}

variable "cloudfront_distribution_id" {
  description = "Distribution ID of the Cloudfront instance to monitor. For example, EDFDVBD6EXAMPLE."
  type        = string
}

variable "cloudfront_5xx_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "cloudfront_5xx_threshold" {
  description = "5xx error rate greater than this to send alarm. Must be in decimal form: 10% = 0.10. Example: '0.0'."
  type        = string
}

variable "cloudfront_cache_hit_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "cloudfront_cache_hit_threshold" {
  description = "Cache hits ratio below which to send alarm. Measured in percentage. Example: '99'."
  type        = string
}

variable "cloudfront_origin_latency_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "cloudfront_origin_latency_threshold" {
  description = "Max origin latency to send alarm. Measured in milliseconds. Example: '5000'."
  type        = string
}

