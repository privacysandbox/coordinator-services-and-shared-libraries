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

variable "project_id" {
  description = "GCP Project ID in which this module will be created."
  type        = string
}

variable "environment" {
  description = "Environment where this service is deployed (e.g. dev, prod)."
  type        = string
}

variable "region" {
  description = "Region the created resources."
  type        = string
}

variable "enable_domain_management" {
  description = "Manage domain SSL cert creation and routing for this service."
  type        = bool
}

variable "encryption_key_domain" {
  description = "Domain to use to create a managed SSL cert for this service."
  type        = string
}

variable "encryption_key_service_cloudfunction_memory_mb" {
  description = "Memory size in MB for cloudfunction."
  type        = number
}

variable "encryption_key_service_cloudfunction_min_instances" {
  description = "The minimum number of function instances that may coexist at a given time."
  type        = number
}

variable "encryption_key_service_cloudfunction_max_instances" {
  description = "The maximum number of function instances that may coexist at a given time."
  type        = number
}

variable "encryption_key_service_request_concurrency" {
  description = "The maximum number of request to allow to be concurrently processed by a function instance."
  type        = number
}

variable "encryption_key_service_cpus" {
  description = "The number of CPUs used in a single container instance."
  type        = number
}

variable "cloudfunction_timeout_seconds" {
  description = "Number of seconds after which a function instance times out."
  type        = number
}

variable "spanner_database_name" {
  description = "Name of the KeyDb Spanner database."
  type        = string
}

variable "spanner_instance_name" {
  description = "Name of the KeyDb Spanner instance."
  type        = string
}

variable "allowed_operator_user_group" {
  description = "Google group of allowed operators to which to give API access."
  type        = string
}

variable "cloud_run_revision_force_replace" {
  description = "Whether to create a new Cloud Run revision for every deployment."
  type        = bool
}

variable "private_key_service_image" {
  description = "The container image of Cloud Run service deployment for Private Key Service."
  type        = string
}

variable "private_key_service_custom_audiences" {
  description = "List of custom audiences for Private Key Service on Cloud Run."
  type        = list(string)
}

variable "private_key_service_canary_percent" {
  description = "Target traffic percentage for the latest Cloud Run revision of Private Key Service."
  type        = number

  validation {
    condition     = var.private_key_service_canary_percent >= 0 && var.private_key_service_canary_percent <= 100
    error_message = "The traffic percent must be an integer between 0 and 100, inclusive."
  }
}

################################################################################
# OpenTelemetry Variables
################################################################################

variable "export_otel_metrics" {
  description = "Use OpenTelemetry to export metrics from Encryption Key Service."
  type        = bool
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

variable "encryption_key_ddos_thresholds" {
  description = "An object containing adaptive protection threshold configuration values for Encryption Key Service."
  type = object({
    name                               = string
    detection_load_threshold           = number
    detection_absolute_qps             = number
    detection_relative_to_baseline_qps = number
  })
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
