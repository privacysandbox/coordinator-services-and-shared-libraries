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

  validation {
    condition     = length(var.environment) <= 18
    error_message = "The max length of the environment name is 18 characters."
  }
}

################################################################################
# Artifact Registy Variables.
################################################################################

variable "pbs_artifact_registry_repository_name" {
  description = "The PBS Artifact Registry repository name."
  type        = string
  nullable    = false
  default     = ""
}

variable "pbs_image_override" {
  description = "The absolute location of the PBS container (including the tag) which will override the derived location"
  type        = string
  nullable    = false
  default     = ""
}

################################################################################
# Cloud Storage Variables.
################################################################################

variable "pbs_cloud_storage_journal_bucket_force_destroy" {
  description = "Whether to force destroy the bucket even if it is not empty."
  type        = bool
  default     = false
}

variable "pbs_cloud_storage_journal_bucket_versioning" {
  description = "Enable bucket versioning."
  type        = bool
  default     = true
}

################################################################################
# Spanner Variables.
################################################################################

# Similar to Point-in-time recovery for AWS DynamoDB
# Must be between 1 hour and 7 days. Can be specified in days, hours, minutes, or seconds.
# eg: 1d, 24h, 1440m, and 86400s are equivalent.
variable "pbs_auth_spanner_database_retention_period" {
  description = "Duration to maintain table versioning for point-in-time recovery."
  type        = string
  nullable    = false
}

variable "pbs_auth_spanner_instance_processing_units" {
  description = "Spanner's compute capacity. 1000 processing units = 1 node and must be set as a multiple of 100."
  type        = number
  nullable    = false
  default     = 1000
}

variable "pbs_auth_spanner_database_deletion_protection" {
  description = "Prevents destruction of a table when it is not empty."
  type        = bool
  nullable    = false
  default     = true
}

# Similar to Point-in-time recovery for AWS DynamoDB
# Must be between 1 hour and 7 days. Can be specified in days, hours, minutes, or seconds.
# eg: 1d, 24h, 1440m, and 86400s are equivalent.
variable "pbs_spanner_database_retention_period" {
  description = "Duration to maintain table versioning for point-in-time recovery."
  type        = string
  nullable    = false
  default     = "30d"
}

variable "pbs_spanner_instance_processing_units" {
  description = "Spanner's compute capacity. 1000 processing units = 1 node and must be set as a multiple of 100."
  type        = number
  default     = null
}

variable "pbs_spanner_database_deletion_protection" {
  description = "Prevents destruction of a table when it is not empty."
  type        = bool
  nullable    = false
  default     = true
}

variable "pbs_spanner_autoscaling_config" {
  description = "Defines the auto-scaling config for Spanner."
  type = object({
    max_nodes                             = number
    min_nodes                             = number
    high_priority_cpu_utilization_percent = number
    storage_utilization_percent           = number
  })
  default = null
}

################################################################################
# PBS Auth Variables.
################################################################################

variable "auth_cloud_function_handler_path" {
  description = "The path to where the cloud function handler file is read from."
  type        = string
  nullable    = false
  default     = ""
}

variable "pbs_auth_cloudfunction_min_instances" {
  description = "The minimum number of function instances that may coexist at a given time."
  type        = number
  nullable    = false
}

variable "pbs_auth_cloudfunction_max_instances" {
  description = "The maximum number of function instances that may coexist at a given time."
  type        = number
  nullable    = false
}

variable "pbs_auth_cloudfunction_instance_concurrency" {
  description = "The maximum number of concurrent requests that one function instance can handle."
  type        = number
  nullable    = false
  default     = 1
}

variable "pbs_auth_cloudfunction_instance_available_cpu" {
  description = "The maximum number of CPUs that will be available to a function instance."
  type        = string
  # If not specified, GCP infra will pick a sensible value based on the memory selection.
  default = null
}

variable "pbs_auth_cloudfunction_memory_mb" {
  description = "Memory size in MB for cloudfunction."
  type        = number
  nullable    = false
  default     = 512
}

variable "pbs_auth_cloudfunction_timeout_seconds" {
  description = "Number of seconds after which a function instance times out."
  type        = number
  nullable    = false
  default     = 60
}

variable "pbs_auth_package_bucket_force_destroy" {
  description = "Whether to force destroy the storage bucket that holds the cloud function artifacts."
  type        = bool
  default     = false
}

variable "pbs_auth_allowed_principals" {
  description = <<-EOT
    The email addresses of the principals that are allowed to execute this
    function. The format of the entry can be found here:
    https://registry.terraform.io/providers/hashicorp/google/latest/docs/resources/cloud_run_service_iam
    e.g.
    user:email@domain.com
    group:mygroup@groups.com
  EOT

  type     = list(string)
  nullable = false
}

################################################################################
# PBS Container Variables.
################################################################################

variable "pbs_image_tag" {
  type     = string
  nullable = false
}

variable "pbs_application_environment_variables" {
  description = "Environment variables to be set for the application running on this environment."
  type        = list(map(string))
  nullable    = false
  default     = [{}]
}

################################################################################
# PBS VM Variables.
################################################################################

variable "machine_type" {
  type     = string
  nullable = false
}

variable "root_volume_size_gb" {
  type     = string
  nullable = false
}

variable "pbs_cloud_logging_enabled" {
  description = "Enables cloud logging of the PBS instances."
  type        = bool
  nullable    = false
  default     = true
}

variable "pbs_cloud_monitoring_enabled" {
  description = "Enables cloud monitoring of the PBS instances."
  type        = bool
  nullable    = false
  default     = true
}

variable "pbs_instance_allow_ssh" {
  description = "Enables SSH to the PBS instances."
  type        = bool
  nullable    = false
  default     = false
}

variable "pbs_service_account_email" {
  description = "The email of the service account to be used for the PBS instance permissions."
  type        = string
  nullable    = false
}

variable "pbs_remote_coordinator_service_account_email" {
  description = "The email of the service account for the PBS remote coordinator."
  type        = string
  nullable    = false
  default     = ""
}

variable "pbs_custom_vm_tags" {
  description = "List of custom tags to be set on the PBS VM instances."
  type        = list(string)
  nullable    = false
  default     = []
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
# PBS Cloud Run Variables.
################################################################################

variable "pbs_cloud_run_min_instances" {
  description = "Minimum instances for Cloud Run PBS"
  type        = number
  nullable    = false
}

variable "pbs_cloud_run_max_instances" {
  description = "Max instances for Cloud Run PBS"
  type        = number
  nullable    = false
}

variable "pbs_cloud_run_max_concurrency" {
  description = "The maximum number of concurrent requests per Cloud Run PBS instance."
  type        = number
  nullable    = false
  default     = 1000
}

variable "deploy_pbs_cloud_run" {
  description = "If true, a Cloud Run PBS backend will be instantiated but not linked to the PBS load balancer"
  type        = bool
  nullable    = false
  default     = false
}

################################################################################
# Network Variables.
################################################################################

variable "parent_domain_project" {
  description = "The project owning the parent domain name used for this PBS environment. NOTE: The hosted zone must exist."
  type        = string
  nullable    = false
  default     = ""
}

variable "parent_domain_name" {
  description = "The parent domain name used for this PBS environment. NOTE: The hosted zone must exist."
  type        = string
  nullable    = false
  default     = ""
}

variable "service_subdomain" {
  description = "Subdomain to use with the parent_domain_name to designate the PBS endpoint."
  type        = string
  default     = "mp-pbs"
}

variable "service_subdomain_suffix" {
  description = "When set, the value replaces `-$${var.environment}` as the service subdomain suffix."
  type        = string
  default     = null
}

variable "pbs_main_port" {
  description = "The main port to map for PBS."
  type        = number
  nullable    = false
  default     = 8080
}

variable "vpc_network_id" {
  description = "The VPC ID. If left blank, the default network will be used."
  type        = string
  nullable    = false
  default     = ""
}

variable "vpc_subnet_id" {
  description = "The VPC subnetwork ID. Must be provided if using a custom VPC."
  type        = string
  nullable    = false
  default     = ""
}

variable "enable_public_ip_address" {
  description = "Whether to enable a public IP address on the created instances."
  type        = bool
  nullable    = false
  default     = false
}

variable "enable_domain_management" {
  description = "value"
  type        = bool
  nullable    = false
  default     = false
}

variable "pbs_tls_alternate_names" {
  description = "PBS Subject Alternative Names for the TLS cert"
  type        = list(string)
  nullable    = true
  default     = null
}

################################################################################
# Health Check Variables.
################################################################################

variable "enable_health_check" {
  description = "Whether to enable the managed instance group health check."
  type        = bool
  nullable    = false
  default     = true
}

################################################################################
# URL Map Variables.
################################################################################

variable "pbs_cloud_run_traffic_percentage" {
  description = "Specifies the percent of traffic sent to Cloud Run PBS."
  type        = number
  nullable    = false
  default     = 0
}

variable "enable_pbs_cloud_run" {
  description = "If true, the Cloud Run PBS backend will be linked to the PBS load balancer and will be able to serve traffi"
  type        = bool
  nullable    = false
  default     = false
}
