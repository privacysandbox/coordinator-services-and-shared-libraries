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
# Function Variables
################################################################################

variable "key_storage_cloudfunction_name" {
  type        = string
  description = "Name of cloud function."
}

variable "package_bucket_name" {
  description = "Name of bucket containing cloudfunction jar."
  type        = string
}

variable "key_storage_service_zip" {
  description = <<-EOT
          Key storage service cloud function path. If not provided defaults to locally built jar file.
        Build with `bazel build //coordinator/terraform/gcp/applications/multipartykeyhosting_secondary:all`.
      EOT
  type        = string
}

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

################################################################################
# Alarm Variables.
################################################################################

variable "alarms_enabled" {
  description = "Enable alarms for this service."
  type        = bool
}

variable "notification_channel_id" {
  description = "Notification channel to which to send alarms."
  type        = string
}

variable "alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = string
}

variable "alarm_duration_sec" {
  description = "Amount of time (in seconds) after which to send alarm if conditions are met. Must be in minute intervals. Example: '60','120'."
  type        = string
}

variable "cloudfunction_error_threshold" {
  description = "Error count greater than this to send alarm. Example: 0."
  type        = string
}

variable "cloudfunction_max_execution_time_max" {
  description = "Max execution time in ms to send alarm. Example: 9999."
  type        = string
}

variable "cloudfunction_5xx_threshold" {
  description = "Cloud Function 5xx error count greater than this to send alarm. Example: 0."
  type        = string
}

variable "lb_max_latency_ms" {
  description = "Load Balancer max latency to send alarm. Measured in milliseconds. Example: 5000."
  type        = string
}

variable "lb_5xx_threshold" {
  description = "Load Balancer 5xx error count greater than this to send alarm. Example: 0."
  type        = string
}
