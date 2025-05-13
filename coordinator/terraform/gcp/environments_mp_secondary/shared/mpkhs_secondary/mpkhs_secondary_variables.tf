# Copyright 2024 Google LLC
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
# Global Variables.
################################################################################

variable "project_id" {
  description = "GCP Project ID in which this module will be created."
  type        = string
}

variable "wipp_project_id_override" {
  description = "Override the GCP Project ID in which workload identity pool (wip) and workload identity pool provider (wipp) will be created in. The value of `project_id` is used by default."
  type        = string
  default     = null
}

variable "wip_allowed_service_account_project_id_override" {
  description = "Override the GCP Project ID in which WIP allowed service account will be created in. The value of `project_id` is used by default."
  type        = string
  default     = null
}

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production."
  type        = string
}

variable "primary_region" {
  description = "Region where all services will be created."
  type        = string
}

variable "secondary_region" {
  description = "Region where all services will be replicated."
  type        = string
}

variable "enable_audit_log" {
  description = "Enable Data Access Audit Logging for projects containing the WIP and Service Accounts."
  type        = bool
  default     = false
}

################################################################################
# Cross-cloud AWS variables
################################################################################

variable "aws_xc_enabled" {
  description = "Enable cross-cloud support for AWS"
  type        = bool
  default     = false
}

variable "aws_kms_key_encryption_key_arn" {
  description = "ARN of the AWS KMS key encryption key"
  type        = string
  default     = null
}

variable "aws_kms_key_encryption_key_role_arn" {
  description = "ARN of AWS IAM role used for encrypting with AWS KMS key encryption key"
  type        = string
  default     = null
}

################################################################################
# Spanner Variables.
################################################################################

variable "spanner_instance_config" {
  description = "Config value for the Spanner Instance."
  type        = string
}

variable "spanner_processing_units" {
  description = "Spanner's compute capacity. 1000 processing units = 1 node and must be set as a multiple of 100."
  type        = number
  default     = 1000
}

# Similar to Point-in-time recovery for AWS DynamoDB
# Must be between 1 hour and 7 days. Can be specified in days, hours, minutes, or seconds.
# eg: 1d, 24h, 1440m, and 86400s are equivalent.
variable "key_db_retention_period" {
  description = "Duration to maintain table versioning for point-in-time recovery."
  type        = string
  default     = "7d"
}

################################################################################
# Routing Variables.
################################################################################

variable "enable_domain_management" {
  description = "Manage domain SSL cert creation and routing for encryption key and key storage services."
  type        = bool
  default     = false
}

variable "parent_domain_name" {
  description = <<-EOT
    Custom domain name to register and use with key hosting APIs.
    Default to null so it does not have to be populated when enable_domain_management = false".
  EOT
  type        = string
  default     = null
}

variable "parent_domain_name_project" {
  description = <<-EOT
    Project ID where custom domain name hosted zone is located.
    Default to null so it does not have to be populated when enable_domain_management = false".
  EOT
  type        = string
  default     = null
}

variable "service_subdomain_suffix" {
  description = "When set, the value replaces `-$${var.environment}` as the service subdomain suffix."
  type        = string
  default     = null
}

variable "encryption_key_service_subdomain" {
  description = "Subdomain to use with parent_domain_name to designate the encryption key service."
  type        = string
  default     = "privatekeyservice"
}

variable "key_storage_service_subdomain" {
  description = "Subdomain to use with parent_domain_name to designate the key storage service."
  type        = string
  default     = "keystorageservice"
}

################################################################################
# Cloud Function Variables.
################################################################################

variable "cloudfunction_timeout_seconds" {
  description = "Number of seconds after which a function instance times out."
  type        = number
  default     = 60
}

variable "encryption_key_service_cloudfunction_memory_mb" {
  description = "Memory size in MB for encryption key cloud function."
  type        = number
}

variable "encryption_key_service_cloudfunction_min_instances" {
  description = "The minimum number of function instances that may coexist at a given time."
  type        = number
  default     = 1
}

variable "encryption_key_service_cloudfunction_max_instances" {
  description = "The maximum number of function instances that may coexist at a given time."
  type        = number
  default     = 100
}

variable "encryption_key_service_request_concurrency" {
  description = "The maximum number of request to allow to be concurrently processed by a function instance."
  type        = number
  default     = 80
}

variable "encryption_key_service_cpus" {
  description = "The number of CPUs used in a single container instance."
  type        = number
  default     = 1
}

variable "key_storage_service_cloudfunction_memory_mb" {
  description = "Memory size in MB for key storage cloud function."
  type        = number
}

variable "key_storage_service_cloudfunction_min_instances" {
  description = "The minimum number of function instances that may coexist at a given time."
  type        = number
  default     = 1
}

variable "key_storage_service_cloudfunction_max_instances" {
  description = "The maximum number of function instances that may coexist at a given time."
  type        = number
  default     = 100
}

################################################################################
# Cloud Run Variables.
################################################################################

variable "cloud_run_revision_force_replace" {
  description = "Whether to create a new Cloud Run revision for every deployment."
  type        = bool
  default     = false
}

variable "private_key_service_image" {
  description = "The container image of Cloud Run service deployment for Private Key Service."
  type        = string
  default     = null
}

variable "private_key_service_custom_audiences" {
  description = "List of custom audiences for Private Key Service on Cloud Run."
  type        = list(string)
  default     = []
}

variable "key_storage_service_image" {
  description = "The container image of Cloud Run service deployment for Key Storage Service."
  type        = string
  default     = null
}

variable "key_storage_service_custom_audiences" {
  description = "List of custom audiences for Key Storage Service on Cloud Run."
  type        = list(string)
  default     = []
}

################################################################################
# Workload Identity Pool Provider Variables.
################################################################################

variable "allowed_wip_iam_principals" {
  description = "List of allowed IAM principals."
  type        = list(string)
  default     = []
}

variable "allowed_wip_user_group" {
  description = "Google Group to manage allowed coordinator users. Deprecated - use allowed_wip_iam_principals instead."
  type        = string
  default     = null
}

variable "allowed_operator_user_group" {
  description = "Google group of allowed operators to which to give API access."
  type        = string
}

variable "enable_attestation" {
  description = "Enable attestation requirement for this Workload Identity Pool. Assertion TEE vars required."
  type        = bool
  default     = true
}

variable "assertion_tee_swname" {
  description = "Software Running TEE. Example: 'CONFIDENTIAL_SPACE'."
  type        = string
  default     = "CONFIDENTIAL_SPACE"
}

variable "assertion_tee_support_attributes" {
  description = "Required support attributes of the Confidential Space image. Example: \"STABLE\", \"LATEST\". Empty for debug images."
  type        = list(string)
  default     = ["STABLE"]
}

variable "assertion_tee_container_image_reference_list" {
  description = "List of acceptable image names TEE can run."
  type        = list(string)
  default     = []
}

variable "assertion_tee_container_image_hash_list" {
  description = "List of acceptable binary hashes TEE can run."
  type        = list(string)
  default     = []
}

################################################################################
# OpenTelemetry Variables
################################################################################

variable "export_otel_metrics" {
  description = "Enable exporting OTEL metrics from MPKHS."
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
  default     = false
}

variable "use_adaptive_protection" {
  description = "Whether Cloud Armor Adaptive Protection is being used or not."
  type        = bool
  default     = false
}

variable "encryption_key_security_policy_rules" {
  description = "Set of objects to define as security policy rules for Encryption Key Service."
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
  default = []
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
  default = []
}

variable "enable_cloud_armor_alerts" {
  description = <<-EOT
    Enable alerts and incidents for Cloud Armor.

    This turns on alerts and incidents for Cloud Armor in the Cloud
    Monitoring dashboards, but does not turn on alert notification messages.
  EOT
  type        = bool
  default     = false
}

variable "enable_cloud_armor_notifications" {
  description = <<-EOT
    Enable alert notifications for Cloud Armor Alerts.

    This turns on notification messages when Cloud Armor alerts fire. Note
    alerts can only fire when enable_cloud_armor_alerts is true, and it
    requires cloud_armor_notification_channel_id to be set.
  EOT
  type        = bool
  default     = false
}

variable "cloud_armor_notification_channel_id" {
  description = <<-EOT
    Notification channel to send Cloud Armor alert notifications to.

    This only needs to be set when enable_cloud_armor_alerts and
    enable_cloud_armor_notifications are both true, and must be set to a valid
    google_monitoring_notification_channel resource id.
  EOT
  type        = string
  default     = null
}
