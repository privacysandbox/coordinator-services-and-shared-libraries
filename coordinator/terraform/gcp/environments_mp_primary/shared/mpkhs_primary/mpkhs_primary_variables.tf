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
# Global Variables.
################################################################################

variable "project_id" {
  description = "GCP Project ID in which this module will be created."
  type        = string
}

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production."
  type        = string
}

variable "allowed_operator_user_group" {
  description = "Google group of allowed operators to which to give API access."
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

################################################################################
# Global Alarm Variables.
################################################################################

variable "alarms_enabled" {
  description = "Enable alarms for mpkhs services."
  type        = bool
  default     = false
}

variable "alarms_notification_email" {
  description = "Email to receive alarms for mpkhs services. Default to null so it is not required when alarms_enabled = false."
  type        = string
  default     = null
}

################################################################################
# Spanner Variables.
################################################################################

variable "spanner_instance_config" {
  description = "Config value for the Spanner Instance. Example: 'nam10'."
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
# Load Balancer Variables.
################################################################################

variable "public_key_load_balancer_logs_enabled" {
  description = "Whether to enable logs for Load Balancer in Public Key Hosting Service."
  type        = bool
  default     = false
}

################################################################################
# Key Generation Variables.
################################################################################

variable "key_generation_image" {
  description = "The Key Generation Application docker image."
  type        = string
}

variable "key_generation_count" {
  description = "Number of keys to generate at a time."
  type        = number
  default     = 5

  validation {
    condition     = var.key_generation_count > 0
    error_message = "Must be greater than 0."
  }
}

variable "key_generation_validity_in_days" {
  description = "Number of days keys will be valid. Should be greater than generation days for failover validity."
  type        = number
  default     = 8

  validation {
    condition     = var.key_generation_validity_in_days > 0
    error_message = "Must be greater than 0."
  }
}

variable "key_generation_ttl_in_days" {
  description = "Keys will be deleted from the database this number of days after creation time."
  type        = number
  default     = 365

  validation {
    condition     = var.key_generation_ttl_in_days > 0
    error_message = "Must be greater than 0."
  }
}

variable "key_generation_cron_schedule" {
  description = <<-EOT
    Frequency for key generation cron job. Must be valid cron statement. Default is every 6 hours on the hour.
    See documentation for more details: https://cloud.google.com/scheduler/docs/configuring/cron-job-schedules
  EOT
  type        = string
  default     = "0 */6 * * *"
}

variable "key_generation_cron_time_zone" {
  description = "Time zone to be used with cron schedule."
  type        = string
  default     = "America/Los_Angeles"
}

variable "instance_disk_image" {
  description = "The image from which to initialize the key generation instance disk for TEE."
  type        = string
  default     = "confidential-space-images/confidential-space-debug"
}

variable "enable_key_generation" {
  description = "Whether to enable the key generation cloud scheduler job for key rotation."
  type        = bool
  default     = true
}

variable "key_generation_logging_enabled" {
  description = "Whether to enable logging for Key Generation instance."
  type        = bool
  default     = true
}

variable "key_generation_monitoring_enabled" {
  description = "Whether to enable monitoring for Key Generation instance."
  type        = bool
  default     = true
}

variable "key_generation_tee_restart_policy" {
  description = "The TEE restart policy. Currently only supports Never."
  type        = string
  default     = "Never"
}

variable "key_generation_tee_allowed_sa" {
  description = "The service account provided by Coordinator B for key generation instance to impersonate."
  type        = string
}

variable "key_generation_undelivered_messages_threshold" {
  description = "Total Queue Messages greater than this to send alert."
  type        = number
  default     = 1
}

variable "key_generation_alignment_period" {
  description = "Alignment period of key generation alert metrics in seconds. This value should match the period of the cron schedule."
  type        = number
  # This value should match the period of the cron schedule.
  # Used for the alignment period of alert metrics.
  # Max 81,000 seconds due to the max GCP metric-threshold evaluation period
  # (23 hours, 30 minutes) plus an extra hour to allow fluctuations in
  # execution time.
  default = 60 * 60 * 6

  validation {
    # This value should match the period of the cron schedule.
    # Used for the alignment period of alert metrics.
    # Max 81,000 seconds due to the max GCP metric-threshold evaluation period
    # (23 hours, 30 minutes) plus an extra hour to allow fluctuations in
    # execution time.
    condition     = var.key_generation_alignment_period > 0 && var.key_generation_alignment_period < 81000
    error_message = "Must be between 0 and 81,000 seconds."
  }
}

variable "key_gen_instance_force_replace" {
  description = "Whether to force key generation instance replacement for every deployment."
  type        = bool
  default     = false
}

variable "peer_coordinator_kms_key_uri" {
  description = "KMS key URI for peer coordinator."
  type        = string
}

variable "key_storage_service_base_url" {
  description = "Base url for key storage service for peer coordinator."
  type        = string
}

variable "key_storage_service_cloudfunction_url" {
  description = "Cloud function url for peer coordinator."
  type        = string
}

variable "peer_coordinator_wip_provider" {
  description = "Peer coordinator wip provider address."
  type        = string
}

variable "peer_coordinator_service_account" {
  description = "Service account generated from peer coordinator."
  type        = string
}

variable "key_id_type" {
  description = "Key ID Type"
  type        = string
  default     = null
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
# Routing Variables.
################################################################################

variable "enable_domain_management" {
  description = "Manage domain SSL cert creation and routing for public and encryption key services."
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

variable "public_key_service_subdomain" {
  description = "Subdomain to use with parent_domain_name to designate the public key service."
  type        = string
  default     = "publickeyservice"
}

variable "public_key_service_alternate_domain_names" {
  description = "A list of alternate domain names for the public key service for which to add as a subject alternative name in the SSL certificate. Eg: [service-example.com]"
  type        = list(string)
  default     = []
}

variable "encryption_key_service_subdomain" {
  description = "Subdomain to use with parent_domain_name to designate the encryption key service."
  type        = string
  default     = "privatekeyservice"
}

variable "enable_public_key_alternative_domain" {
  description = "Set to true to enable the creation of alternative domain certificates and related resources."
  type        = bool
  default     = false
}

variable "disable_public_key_ssl_cert" {
  description = "Disable the SSL certificate when all current certificates are migrated to cert manager cert"
  type        = bool
  default     = false
}

variable "remove_public_key_ssl_cert" {
  description = "Remove the SSL certificate when all current certificates are migrated to cert manager cert"
  type        = bool
  default     = false
}

################################################################################
# Cloud Function Variables.
################################################################################

variable "cloudfunction_timeout_seconds" {
  description = "Number of seconds after which a function instance times out."
  type        = number
  default     = 60
}

variable "get_public_key_cloudfunction_memory_mb" {
  description = "Memory size in MB for public key cloud function."
  type        = number
}

variable "encryption_key_service_cloudfunction_memory_mb" {
  description = "Memory size in MB for encryption key cloud function."
  type        = number
}

variable "get_public_key_cloudfunction_min_instances" {
  description = "The minimum number of function instances that may coexist at a given time."
  type        = number
  default     = 1
}

variable "encryption_key_service_cloudfunction_min_instances" {
  description = "The minimum number of function instances that may coexist at a given time."
  type        = number
  default     = 1
}

variable "get_public_key_cloudfunction_max_instances" {
  description = "The maximum number of function instances that may coexist at a given time."
  type        = number
  default     = 100
}

variable "encryption_key_service_cloudfunction_max_instances" {
  description = "The maximum number of function instances that may coexist at a given time."
  type        = number
  default     = 100
}

variable "get_public_key_request_concurrency" {
  description = "The maximum number of request to allow to be concurrently processed by a function instance."
  type        = number
  default     = 80
}

variable "encryption_key_service_request_concurrency" {
  description = "The maximum number of request to allow to be concurrently processed by a function instance."
  type        = number
  default     = 80
}

variable "get_public_key_cpus" {
  description = "The number of CPUs used in a single container instance."
  type        = number
  default     = 2
}

variable "encryption_key_service_cpus" {
  description = "The number of CPUs used in a single container instance."
  type        = number
  default     = 1
}

variable "application_name" {
  description = "Application name to separate serving path for public key service endpoint. Eg: aggregation-service."
  type        = string
  default     = "default"
}

################################################################################
# Cloud Run Variables.
################################################################################

variable "cloud_run_revision_force_replace" {
  description = "Whether to create a new Cloud Run revision for every deployment."
  type        = bool
  default     = false
}

variable "public_key_service_image" {
  description = "The container image of Cloud Run service deployment for Public Key Service."
  type        = string
  default     = null
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

variable "public_key_service_canary_percent" {
  description = "Target traffic percentage for the latest Cloud Run revision of Public Key Service."
  type        = number
  default     = 100
}

variable "private_key_service_canary_percent" {
  description = "Target traffic percentage for the latest Cloud Run revision of Private Key Service."
  type        = number
  default     = 100
}

################################################################################
# Key Management Variables.
################################################################################

variable "enable_get_public_key_cdn" {
  description = "Enable Get Public Key API CDN."
  type        = bool
  default     = true
}

variable "get_public_key_cloud_cdn_default_ttl_seconds" {
  description = "Default CDN TTL seconds to use when no cache headers are present."
  type        = number
  default     = 604800
}

variable "get_public_key_cloud_cdn_max_ttl_seconds" {
  description = "Maximum CDN TTL seconds that cache header directive cannot surpass."
  type        = number
  default     = 604800
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

variable "public_key_security_policy_rules" {
  description = "Set of objects to define as security policy rules for Public Key Service."
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
