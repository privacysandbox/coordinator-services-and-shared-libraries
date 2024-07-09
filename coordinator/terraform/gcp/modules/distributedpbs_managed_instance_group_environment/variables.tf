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

variable "project" {
  description = "The GCP project name to use."
  type        = string
  nullable    = false
}

variable "region" {
  description = "Region where all resources will be created."
  type        = string
  nullable    = false
}

variable "environment" {
  description = "Description for the environment. Used to prefix resources. e.g. dev, staging, production."
  type        = string
  nullable    = false
}

################################################################################
# Artifact Registy Variables.
################################################################################

variable "pbs_artifact_registry_repository_name" {
  description = "The PBS Artifact Registry repository name."
  type        = string
  nullable    = false
}

################################################################################
# Cloud Storage Variables.
################################################################################

variable "pbs_cloud_storage_journal_bucket_name" {
  description = "The PBS Cloud Storage bucket name for journaling."
  type        = string
  nullable    = false
}

################################################################################
# Spanner Variables.
################################################################################

variable "pbs_spanner_instance_name" {
  description = "Name of the PBS Spanner instance."
  type        = string
  nullable    = false
}

variable "pbs_spanner_database_name" {
  description = "Name of the PBS Spanner database."
  type        = string
  nullable    = false
}

variable "pbs_spanner_budget_key_table_name" {
  description = "Name of the PBS budget key table."
  type        = string
  nullable    = false
}

variable "pbs_spanner_partition_lock_table_name" {
  description = "Name of the PBS partition lock table."
  type        = string
  nullable    = false
}

################################################################################
# PBS Container Variables.
################################################################################

variable "pbs_image_tag" {
  type     = string
  nullable = false
}

variable "pbs_auth_audience_url" {
  description = "The URL for the PBS auth service."
  type        = string
}

variable "pbs_application_environment_variables" {
  description = "Environment variables to be set for the application running on this environment."
  type        = list(object({ name = string, value = string }))
  nullable    = false
}

################################################################################
# PBS VM Variables.
################################################################################

variable "machine_type" {
  type     = string
  nullable = false
}

variable "root_volume_size_gb" {
  type     = number
  nullable = false
}

variable "pbs_cloud_logging_enabled" {
  type     = bool
  nullable = false
}

variable "pbs_cloud_monitoring_enabled" {
  type     = bool
  nullable = false
}

variable "pbs_service_account_email" {
  description = "The email of the service account to be used for the PBS instance permissions."
  type        = string
  nullable    = false
}

variable "pbs_custom_vm_tags" {
  description = "List of custom tags to be set on the PBS VM instances."
  type        = list(string)
  nullable    = false
}

variable "pbs_autoscaling_policy" {
  description = "Auto-scaling policy for PBS instances."
  type = object({
    min_replicas           = number
    max_replicas           = number
    cpu_utilization_target = number
  })
  default = null
}

################################################################################
# Network Variables.
################################################################################

variable "vpc_network_id" {
  description = "The VPC ID. If left blank, the default network will be used."
  type        = string
  nullable    = false
}

variable "vpc_subnet_id" {
  description = "The VPC subnetwork ID. Must be provided if using a custom VPC."
  type        = string
  nullable    = false
}

variable "network_target_tag" {
  description = "Used to mark the created instances as network targets."
  type        = string
  nullable    = false
}

variable "main_port" {
  description = "The main port for the PBS."
  type        = number
  nullable    = false
}

variable "health_check_port" {
  description = "The port to use for health checks."
  type        = number
  nullable    = false
}

variable "named_ports" {
  description = "The named ports to assign to the generated instances."
  type        = list(object({ name = string, port = number }))
  nullable    = false
}

variable "enable_public_ip_address" {
  description = "Whether to enable a public IP address on the created instances."
  type        = bool
  nullable    = false
}

variable "enable_domain_management" {
  description = "value"
  type        = bool
  nullable    = false
}

variable "pbs_domain" {
  description = "The PBS domain. Unused if domain management is disabled."
  type        = string
  nullable    = false
}

################################################################################
# Health Check Variables.
################################################################################

variable "enable_health_check" {
  description = "Whether to enable the managed instance group health check."
  type        = bool
  nullable    = false
}
