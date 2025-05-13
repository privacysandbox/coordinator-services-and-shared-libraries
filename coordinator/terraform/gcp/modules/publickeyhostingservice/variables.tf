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

variable "regions" {
  description = "Regions for the created resources."
  type        = set(string)
}

variable "enable_domain_management" {
  description = "Manage domain SSL cert creation and routing for this service."
  type        = bool
}

variable "public_key_domain" {
  description = "Domain to use to create a managed SSL cert for this service."
  type        = string
}

variable "public_key_service_alternate_domain_names" {
  description = "A list of alternate domain names for the public key service for which to add as a subject alternative name in the SSL certificate. Eg: [service-example.com]"
  type        = list(string)
}

variable "get_public_key_cloudfunction_memory_mb" {
  description = "Memory size in MB for cloudfunction."
  type        = number
}

variable "get_public_key_cloudfunction_min_instances" {
  description = "The minimum number of function instances that may coexist at a given time."
  type        = number
}

variable "get_public_key_cloudfunction_max_instances" {
  description = "The maximum number of function instances that may coexist at a given time."
  type        = number
}

variable "get_public_key_request_concurrency" {
  description = "The maximum number of request to allow to be concurrently processed by a function instance."
  type        = number
}

variable "get_public_key_cpus" {
  description = "The number of CPUs used in a single container instance."
  type        = number
}

variable "cloudfunction_timeout_seconds" {
  description = "Number of seconds after which a function instance times out."
  type        = number
}

variable "enable_get_public_key_cdn" {
  description = "Enable Get Public Key API CDN"
  type        = bool
}

variable "get_public_key_cloud_cdn_default_ttl_seconds" {
  description = "Default CDN TTL seconds to use when no cache headers are present."
  type        = number
}

variable "get_public_key_cloud_cdn_max_ttl_seconds" {
  description = "Maximum CDN TTL seconds that cache header directive cannot surpass."
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

variable "application_name" {
  description = "Application name to separate serving path for public key service endpoint. Eg: aggregation-service."
  type        = string
}

variable "public_key_load_balancer_logs_enabled" {
  description = "Whether to enable logs for Load Balancer in Public Key Hosting Service."
  type        = bool
}

variable "cloud_run_revision_force_replace" {
  description = "Whether to create a new Cloud Run revision for every deployment."
  type        = bool
}

variable "public_key_service_image" {
  description = "The container image of Cloud Run service deployment for Public Key Service."
  type        = string
}

################################################################################
# OpenTelemetry Variables
################################################################################

variable "export_otel_metrics" {
  description = "Use OpenTelemetry to export metrics from Public Key Service."
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
