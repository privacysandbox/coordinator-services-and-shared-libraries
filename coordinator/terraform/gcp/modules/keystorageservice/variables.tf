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

variable "project_id" {
  description = "GCP Project ID in which this module will be created."
  type        = string
}

variable "environment" {
  type        = string
  description = "Environment where this service is deployed (e.g. dev, prod)."
}

variable "region" {
  type        = string
  description = "Region for load balancer and cloud function."
}

variable "enable_domain_management" {
  description = "Manage domain SSL cert creation and routing for this service."
  type        = bool
}

variable "key_storage_domain" {
  description = "Domain to use to create a managed SSL cert for this service."
  type        = string
}

variable "allowed_wip_iam_principals" {
  description = "List of allowed coordinator IAM principals."
  type        = list(string)
}

variable "allowed_wip_user_group" {
  description = "Google Group to manage allowed coordinator users. Deprecated - use allowed_wip_iam_principals instead."
  type        = string
}

################################################################################
# Cross-cloud AWS variables
################################################################################

variable "aws_xc_enabled" {
  description = "Enable cross-cloud support for AWS"
  type        = bool
}

variable "aws_kms_key_encryption_key_arn" {
  description = "ARN of the AWS KMS key encryption key"
  type        = string
}

variable "aws_kms_key_encryption_key_role_arn" {
  description = "ARN of AWS IAM role used for encrypting with AWS KMS key encryption key"
  type        = string
}

################################################################################
# Function Variables
################################################################################

variable "key_storage_cloudfunction_memory" {
  type        = number
  description = "Memory size in MB for cloud function."
}

variable "key_storage_service_cloudfunction_min_instances" {
  description = "The minimum number of function instances that may coexist at a given time."
  type        = number
}

variable "key_storage_service_cloudfunction_max_instances" {
  description = "The maximum number of function instances that may coexist at a given time."
  type        = number
}

variable "cloudfunction_timeout_seconds" {
  description = "Number of seconds after which a function instance times out."
  type        = number
}

variable "load_balancer_name" {
  description = "Name of the load balancer."
  type        = string
}

variable "spanner_instance_name" {
  type        = string
  description = "Name of the KeyDb Spanner instance."
}

variable "spanner_database_name" {
  type        = string
  description = "Name of the KeyDb Spanner database."
}

variable "key_encryption_key_id" {
  description = "KMS key used to decrypt private keys."
  type        = string
}

variable "cloud_run_revision_force_replace" {
  description = "Whether to create a new Cloud Run revision for every deployment."
  type        = bool
}

variable "key_storage_service_image" {
  description = "The container image of Cloud Run service deployment for Key Storage Service."
  type        = string
}

variable "key_storage_service_custom_audiences" {
  description = "List of custom audiences for Key Storage Service on Cloud Run."
  type        = list(string)
}

################################################################################
# OpenTelemetry Variables
################################################################################

variable "export_otel_metrics" {
  description = "Use OpenTelemetry to export metrics from Key Storage Service."
  type        = bool
  default     = false
}

################################################################################
# Variables for AWS cross-cloud key synchronization.
################################################################################

variable "aws_key_sync_enabled" {
  description = "Enable AWS cross-cloud key synchronization."
  type        = bool
  default     = false
}

variable "aws_key_sync_role_arn" {
  description = "ARN of the AWS role to be assumed for cross-cloud key sync."
  type        = string
  default     = ""
}

variable "aws_key_sync_kms_key_uri" {
  description = "URI of the AWS KMS key used to wrap legacy AWS private keys."
  type        = string
  default     = ""
}

variable "aws_key_sync_keydb_region" {
  description = "Region of the Dynamodb KeyDB table for key sync writes."
  type        = string
  default     = ""
}

variable "aws_key_sync_keydb_table_name" {
  description = "Table name of the Dynamodb KeyDB for key sync writes."
  type        = string
  default     = ""
}

################################################################################
# Cloud Armor Variables
################################################################################

variable "enable_security_policy" {
  description = "Whether to enable the security policy on the backend service(s)."
  type        = bool
}

variable "use_adaptive_protection" {
  description = "Whether Cloud Armor Adaptive Protection is being used or not."
  type        = bool
}

variable "key_storage_ddos_thresholds" {
  description = "An object containing adaptive protection threshold configuration values for Key Storage Service."
  type = object({
    name                               = string
    detection_load_threshold           = number
    detection_absolute_qps             = number
    detection_relative_to_baseline_qps = number
  })
}

variable "key_storage_security_policy_rules" {
  description = "Set of objects to define as security policy rules for Key Storage Service."
  type = set(object({
    description = string
    action      = string
    priority    = number
    preview     = bool
    match = object({
      versioned_expr = string
      config = object({
        src_ip_ranges = list(string)
      })
      expr = object({
        expression = string
      })
    })
  }))
}

variable "enable_cloud_armor_alerts" {
  description = "Enable alerts and incidents for Cloud Armor."
  type        = bool
  default     = false
}

variable "enable_cloud_armor_notifications" {
  description = "Enable alert notifications for Cloud Armor to cloud_armor_notification_channel_id."
  type        = bool
  default     = false
}

variable "cloud_armor_notification_channel_id" {
  description = "Notification channel to send Cloud Armor alert notifications to."
  type        = string
  default     = null
}
