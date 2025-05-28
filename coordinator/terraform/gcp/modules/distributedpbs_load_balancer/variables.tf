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
  description = "The name of the environment e.g. staging."
  type        = string
  nullable    = false
}

variable "region" {
  description = "The GCP region for this deployment."
  type        = string
  nullable    = false
}

variable "project_id" {
  description = "The GCP project ID to use for this deployment."
  type        = string
  nullable    = false
}

variable "parent_domain_name" {
  description = "The parent domain name used for this PBS environment. NOTE: The hosted zone must exist."
  type        = string
}

variable "enable_domain_management" {
  description = "Whether to enable domain management."
  type        = bool
  nullable    = false
}

variable "pbs_domain" {
  description = "The domain used for PBS."
  type        = string
  nullable    = false
}

variable "parent_domain_project" {
  description = "The project owning the parent domain name used for this PBS environment. NOTE: The hosted zone must exist."
  type        = string
}

variable "pbs_ip_address" {
  description = "The PBS IP address."
  type        = string
  nullable    = false
}

variable "pbs_auth_cloudfunction_name" {
  description = "The PBS cloud function name."
  type        = string
  nullable    = false
}

variable "pbs_cloud_run_name" {
  description = "The Cloud Run PBS name."
  type        = string
  nullable    = false
}

variable "pbs_main_port" {
  description = "The port to access the main PBS. Should map to pbs_named_port."
  type        = number
  nullable    = false
}

variable "pbs_vpc_network_id" {
  description = "This is the VPC (google compute network) ID to use for the load balancer."
  type        = string
}

variable "pbs_tls_alternate_names" {
  description = "PBS Subject Alternative Names for the TLS cert"
  type        = list(string)
  nullable    = true
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

variable "pbs_ddos_thresholds" {
  description = "An object containing adaptive protection threshold configuration values for PBS."
  type = object({
    name                               = string
    detection_load_threshold           = number
    detection_absolute_qps             = number
    detection_relative_to_baseline_qps = number
  })
}

variable "pbs_security_policy_rules" {
  description = "Set of objects to define as security policy rules for PBS."
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

variable "certificate_manager_has_prefix" {
  description = "If true, certificate manager resources will be prefixed with the environment name."
  type        = bool
  default     = false
}
