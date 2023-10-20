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

variable "custom_alarm_label" {
  description = "Add any string to the label to help filtering, allowed chars (a-zA-Z_-) max 30 chars"
  type        = string
}

################################################################################
# Lambda Variables
################################################################################

variable "keymanagement_package_bucket" {
  description = "Bucket containing lambda jar"
  type        = string
}

variable "public_key_service_jar" {
  description = "Path to the jar file for lambda function."
  type        = string
}

variable "get_public_key_logging_retention_days" {
  description = "Number of days to keep logs in Cloudwatch."
  type        = number
}

variable "get_public_key_lambda_memory_mb" {
  description = "Memory size in MB for lambda function."
  type        = number
}

variable "dynamo_keydb" {
  description = "Existing KeyDb table name to pass to lambda"
  type = object({
    table_name = string
    arn        = string
    region     = string
  })
}

variable "get_public_key_cache_control_max_seconds" {
  description = "Value of Cache-Control max-age API header. Should reflect the key generation rate."
  type        = number

  validation {
    condition     = var.get_public_key_cache_control_max_seconds > 0
    error_message = "Must be greater than 0."
  }
}

variable "get_public_key_lambda_provisioned_concurrency_enabled" {
  description = "Whether to use use provisioned concurrency for get public key lambda."
  type        = bool
}

variable "get_public_key_lambda_provisioned_concurrency_count" {
  description = "Number of lambda instances to initialize for provisioned concurrency."
  type        = number
}

################################################################################
# API Gateway Variables
################################################################################

variable "api_version" {
  description = "Version of the created APIs. Eg: v1"
  type        = string
}

variable "api_env_stage_name" {
  description = "Stage name for API gateway instances. Must match across environment"
  type        = string
}

variable "application_name" {
  description = "Application name that provide public key service. Eg: aggregation-service."
  type        = string
}

################################################################################
# Alarm Variables
################################################################################

variable "public_key_service_alarms_enabled" {
  description = "Enable alarms for public key service"
  type        = bool
}

variable "get_public_key_sns_topic_arn" {
  description = "SNS topic ARN for alarm actions"
  type        = string
}

variable "get_public_key_alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = string
}

variable "get_public_key_lambda_error_threshold" {
  description = "Error rate greater than this to send alarm. Must be in decimal form: 10% = 0.10. Example: '0.0'."
  type        = string
}

variable "get_public_key_lambda_error_log_threshold" {
  description = "Error log sum greater than this to send alarm. Example: '0'."
  type        = string
}

variable "get_public_key_lambda_max_duration_threshold" {
  description = "Lambda max duration in ms to send alarm. Useful for timeouts. Example: '9999'."
  type        = string
}

variable "get_public_key_api_max_latency_ms" {
  description = "API Gateway max latency to send alarm. Measured in milliseconds. Example: '5000'."
  type        = string
}

variable "get_public_key_5xx_threshold" {
  description = "API Gateway 5xx error rate greater than this to send alarm. Must be in decimal form: 10% = 0.10. Example: '0.0'."
  type        = string
}

################################################################################
# VPC Variables
################################################################################

variable "enable_vpc" {
  description = "Enable the creation of a customer-owned VPC providing network security."
  type        = bool
}

variable "coordinator_vpc_subnet_ids" {
  description = "Ids of secure VPC subnets used by Lambda."
  type        = list(string)
}

variable "coordinator_vpc_security_groups_ids" {
  description = "IDs of secure VPC security groups used by Lambda."
  type        = list(string)
}

variable "keydb_vpc_endpoint_id" {
  description = "ID of VPC endpoint to access keydb."
  type        = string
}
