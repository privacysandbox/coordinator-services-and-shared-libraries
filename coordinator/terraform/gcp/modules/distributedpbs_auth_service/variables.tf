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
  description = "Description for the environment, e.g. dev, staging, production."
  type        = string
}

variable "auth_cloud_function_handler_path" {
  description = "The path to where the cloud function handler package(zip) is read from."
  type        = string
}

variable "region" {
  description = "Region where all resources will be created."
  type        = string
}

variable "pbs_auth_cloudfunction_min_instances" {
  description = "The minimum number of function instances that may coexist at a given time."
  type        = number
}

variable "pbs_auth_cloudfunction_max_instances" {
  description = "The maximum number of function instances that may coexist at a given time."
  type        = number
}

variable "pbs_auth_cloudfunction_instance_concurrency" {
  description = "The maximum number of concurrent requests that one function instance can handle."
  type        = number
}

variable "pbs_auth_cloudfunction_instance_available_cpu" {
  description = "The maximum number of CPUs that will be available to a function instance."
  type        = string
}

variable "pbs_auth_cloudfunction_memory_mb" {
  description = "Memory size in MB for cloudfunction."
  type        = number
}

variable "pbs_auth_cloudfunction_timeout_seconds" {
  description = "Number of seconds after which a function instance times out."
  type        = number
}

variable "pbs_auth_package_bucket_force_destroy" {
  description = "Whether to force destroy the storage bucket that holds the cloud function artifacts."
  type        = bool
}

variable "pbs_auth_spanner_instance_name" {
  description = "The PBS auth spanner instance name."
  type        = string
}

variable "pbs_auth_spanner_database_name" {
  description = "The PBS auth spanner database name."
  type        = string
}

variable "pbs_auth_v2_spanner_table_name" {
  description = "Name of the PBS authorization V2 table."
  type        = string
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

  type = list(string)
}

variable "limit_traffic_to_internal_and_lb" {
  description = "Whether to limit traffic to internal and load balancer only."
  type        = bool
}
