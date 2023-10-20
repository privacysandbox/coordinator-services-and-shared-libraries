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

################################################################################
# Global Variables
################################################################################

variable "environment" {
  description = "environment where this service is deployed. Eg: dev/prod, etc"
  type        = string
}

################################################################################
# API Gateway Variables
################################################################################

variable "api_version" {
  description = "Version of the created APIs. Eg: v1"
  type        = string
}

variable "api_gateway_id" {
  description = "Id of the gateway to use for hosting this service"
  type        = string
}

variable "api_gateway_execution_arn" {
  description = "Execution ARN of the gateway used for hosting this service"
  type        = string
}

################################################################################
# Lambda Variables
################################################################################

variable "keymanagement_package_bucket" {
  description = "Bucket containing lambda jar"
  type        = string
}

variable "encryption_key_service_jar" {
  description = "Path to the jar file for lambda function."
  type        = string
}

variable "dynamo_keydb" {
  description = "The dynamodb table where keys are stored"
  type = object({
    name   = string
    arn    = string
    region = string
  })
}

variable "logging_retention_days" {
  description = "Number of days to keep logs in Cloudwatch"
  type        = number
}

variable "get_encryption_key_lambda_ps_client_shim_enabled" {
  description = <<-EOT
        If true, enables shim to ensure response is compatible with privacy sandbox 0.51.x
        clients. Newer fields such as activationTime is filtered out from the response.
        This temporary variable will be removed in the future.
      EOT
  type        = bool
}

################################################################################
# Alarm Variables
################################################################################

variable "alarms_enabled" {
  description = "Enable alarms for private key service"
  type        = bool
}

variable "sns_topic_arn" {
  description = "SNS topic ARN for alarm actions"
  type        = string
}

variable "alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = string
}

variable "lambda_error_threshold" {
  description = "Error rate greater than this to send alarm. Must be in decimal form: 10% = 0.10. Example: '0.0'."
  type        = string
}

variable "lambda_error_log_threshold" {
  description = "Error log sum greater than this to send alarm. Example: '0'."
  type        = string
}

variable "lambda_max_duration_threshold" {
  description = "Lambda max duration in ms to send alarm. Useful for timeouts. Example: '9999'."
  type        = string
}

variable "custom_alarm_label" {
  description = "Add any string to the label to help filtering, allowed chars (a-zA-Z_-) max 30 chars"
  type        = string
}

################################################################################
# VPC Variables
################################################################################

variable "enable_vpc" {
  description = "Enable the creation of customer-owned VPCs used by Lambda."
  type        = bool
}

variable "private_subnet_ids" {
  description = "IDs of the private subnet used by Lambda."
  type        = list(string)
}

variable "lambda_sg_ids" {
  description = "IDs of the security groups used by Lambda."
  type        = list(string)
}

variable "dynamodb_vpc_endpoint_id" {
  description = "ID of the VPC endpoint to access DynamoDb."
  type        = string
}
