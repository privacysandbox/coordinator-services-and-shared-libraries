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

########################
# DO NOT EDIT MANUALLY #
########################

# This file is meant to be shared across all environments (either copied or
# symlinked). In order to make the upgrade process easier, this file should not
# be modified for environment-specific customization.

################################################################################
# Global Variables.
################################################################################

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production."
  type        = string

  validation {
    condition     = length(var.environment) <= 18
    error_message = "The max length of the environment name is 18 characters."
  }
}

variable "region" {
  description = "Region where all services will be created."
  type        = string
  default     = ""
}

variable "primary_region" {
  description = "Region where all services will be created -- Deprecated, use `region`."
  type        = string
  default     = ""
}

################################################################################
# Permissions
################################################################################

variable "allowed_peer_coordinator_principals" {
  description = "AWS principals to grant access to KeyStorageService and its corresponding KMS key."
  type        = list(string)
  default     = []

  validation {
    condition     = length(var.allowed_peer_coordinator_principals) > 0
    error_message = <<-EOT
       At least one peer coordinator AWS principal must be provided.
       If this is a test environment, consider adding this account's account ID.
    EOT
  }
}

variable "keygeneration_attestation_condition_keys" {
  description = <<-EOT
    Map of Condition Keys for Nitro Enclave attestation to apply to peer coordinator decryption policies.
    Key = Condition key type (PCR0). Value = list of acceptable enclave hashes.
    If map is empty, then no condition is applied and attestation is *not* required for key generation enclave to do decryption.
    EOT
  type        = map(list(string))
  default     = {}
}

################################################################################
# DynamoDB Variables.
################################################################################

variable "keydb_table_name" {
  description = "Name of the DynamoDB table. Defaults to {var.environment}_keydb if unspecified."
  type        = string
  default     = ""
}

variable "enable_dynamo_point_in_time_recovery" {
  description = "Allows DynamoDB table data to be recovered after table deletion."
  type        = bool
  default     = true
}

variable "enable_keydb_ttl" {
  description = "Allows DynamoDB table data to delete keys automatically based on the TtlTime attribute."
  type        = bool
  default     = false
}

variable "keydb_write_capacity" {
  description = "Provisioned write capacity units."
  type        = number
  default     = 5
}

variable "keydb_read_capacity" {
  description = "Provisioned read capacity units."
  type        = number
  default     = 5
}

variable "keydb_autoscaling_read_max_capacity" {
  description = "Maximum read capacity units."
  type        = number
  default     = 500
}

variable "keydb_autoscaling_read_target_percentage" {
  description = "Read capacity target utilization."
  type        = number
  default     = 70
}

variable "keydb_autoscaling_write_max_capacity" {
  description = "Maximum write capacity units."
  type        = number
  default     = 500
}

variable "keydb_autoscaling_write_target_percentage" {
  description = "Write capacity target utilization."
  type        = number
  default     = 70
}

################################################################################
# Lambda Variables.
################################################################################

variable "create_encryption_key_jar" {
  description = <<-EOT
        Path to the Key Storage Service Lambda jar file.

        If provided as a blank string, uses locally built jar file, built with
        `bazel build //coordinator/terraform/aws:multiparty_dist_files`.

        If not provided, uses the relative path in the distribution tarball
        (top level under /dist/...)
      EOT
  type        = string
  default     = "../../../dist/KeyStorageServiceLambda_deploy.jar"
}

variable "encryption_key_service_jar" {
  description = <<-EOT
        Private key service lambda path.

        If provided as a blank string, uses locally built jar file, built with
        `bazel build //coordinator/terraform/aws:multiparty_dist_files`.

        If not provided, uses the relative path in the distribution tarball
        (top level under /dist/...)
      EOT
  type        = string
  default     = "../../../dist/EncryptionKeyServiceLambda_deploy.jar"
}

variable "enable_public_key_signature" {
  description = "Generate a public key signature key for this coordinator and include signatures."
  type        = bool
  default     = true
}

variable "get_encryption_key_lambda_ps_client_shim_enabled" {
  description = <<-EOT
        If true, enables shim to ensure response is compatible with privacy sandbox 0.51.x
        clients. Newer fields such as activationTime is filtered out from the response.
        This temporary variable will be removed in the future.
      EOT
  type        = bool
  default     = false
}

################################################################################
# Routing Variables.
################################################################################

variable "enable_domain_management" {
  description = "Manage domain SSL cert creation and routing for public and private key services."
  type        = bool
}

variable "parent_domain_name" {
  description = "Custom domain name to register and use with key management APIs."
  type        = string
  default     = ""
}

variable "encryption_key_service_subdomain" {
  description = "Subdomain to use with parent_domain_name to designate the private key service."
  type        = string
  default     = "privatekeyservice"
}

variable "api_version" {
  description = "Version of the created APIs. Eg: v1."
  type        = string
}

variable "api_env_stage_name" {
  description = "Stage name for API gateway instances."
  type        = string
  default     = "stage"
}

################################################################################
# Global Alarm Variables.
################################################################################

variable "alarms_enabled" {
  description = "Enable alarms for spkhs services."
  type        = bool
}

variable "alarm_notification_email" {
  description = "Email to receive alarms for public key services."
  type        = string
}

variable "cloudwatch_logging_retention_days" {
  description = "Number of days to keep logs in Cloudwatch."
  type        = number
  default     = 30
}

variable "sns_topic_arn" {
  description = "SNS topic ARN to forward alerts to"
  type        = string
  default     = ""
}

variable "sqs_queue_arn" {
  description = "SQS queue ARN to forward alerts to"
  type        = string
  default     = ""
}

variable "custom_alarm_label" {
  description = "Add any string to the label to help filtering, allowed chars (a-zA-Z_-) max 30 chars"
  type        = string
  default     = ""
}

################################################################################
# Private Key Alarm Variables.
################################################################################

variable "mpkhs_alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = string
  default     = "300"
}

variable "mpkhs_lambda_error_threshold" {
  description = "Error rate greater than this to send alarm. Must be in decimal form: 10% = 0.10. Example: '0.0'."
  type        = string
  default     = "0.0"
}

variable "mpkhs_lambda_error_log_threshold" {
  description = "Error log sum greater than this to send alarm. Example: '0'."
  type        = string
  default     = "0"
}

variable "mpkhs_lambda_max_duration_threshold" {
  description = "Lambda max duration in ms to send alarm. Useful for timeouts. Default 9999ms since lambda time out is set to 10s."
  type        = string
  default     = "9999"
}

variable "mpkhs_api_gw_max_latency_ms" {
  description = "API Gateway max latency to send alarm. Measured in milliseconds. Default 10s because API takes 6-7 seconds on cold start."
  type        = string
  default     = "10000"
}

variable "mpkhs_api_gw_5xx_threshold" {
  description = "API Gateway 5xx error rate greater than this to send alarm. Must be in decimal form: 10% = 0.10. Example: '0.0'."
  type        = string
  default     = "0.0"
}

variable "mpkhs_api_gw_error_ratio_4xx_threshold" {
  description = "API Gateway 4xx error ratio rate greater than this to send alarm. Must be in decimal form: 10% = 0.10. Example: '0.0'."
  type        = string
  default     = "5"
}

variable "mpkhs_api_gw_error_ratio_5xx_threshold" {
  description = "API Gateway 5xx error ratio rate greater than this to send alarm. Must be in decimal form: 10% = 0.10. Example: '0.0'."
  type        = string
  default     = "5"
}

################################################################################
# VPC Variables.
################################################################################

variable "enable_vpc" {
  description = "Enable the creation of customer-owned VPCs used by Lambda."
  type        = bool
  default     = false
}

variable "vpc_cidr" {
  description = "VPC CIDR range for coordinator VPC."
  type        = string
  default     = "10.0.0.0/16"
}

variable "availability_zone_replicas" {
  description = "Number of subnet groups to create over additional availability zones in the region (up to 4 for /16, if supported by AWS region)."
  type        = number
  default     = 3
}

variable "vpc_default_tags" {
  description = "Tags to add to network infrastructure provisioned by this module."
  type        = map(string)
  default     = null
}

variable "enable_vpc_flow_logs" {
  description = "Whether to enable VPC flow logs that end up in a cloudwatch log group."
  type        = bool
  default     = false
}

variable "vpc_flow_logs_traffic_type" {
  description = "The traffic type to log. Either ACCEPT, REJECT or ALL."
  type        = string
  default     = "ALL"
}

variable "vpc_flow_logs_retention_in_days" {
  description = "The number of days to keep the flow logs in CloudWatch."
  type        = number
  default     = 90
}

################################################################################
# Dashboard Variables.
################################################################################

variable "unified_keyhosting_dashboard_time_period_seconds" {
  description = "Time period that acts as window for dashboard metrics. Measured in seconds."
  type        = number
  default     = 60
}
