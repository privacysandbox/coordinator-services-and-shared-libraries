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
}

variable "pbs_image_override" {
  description = "The absolute location of the PBS container (including the tag) which will override the derived location"
  type        = string
  default     = null
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
  description = "The tag of the PBS image stored in the derived location (only set this if pbs_image_override is not set)"
  type        = string
  default     = null
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

variable "pbs_service_account_email" {
  description = "The email of the service account to be used for the PBS instance permissions."
  type        = string
  nullable    = false
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
  description = "The maximum number of concurrent requests per Cloud Run PBS instance"
  type        = number
  nullable    = false
}

################################################################################
# Network Variables.
################################################################################

variable "main_port" {
  description = "The main port for the PBS."
  type        = number
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