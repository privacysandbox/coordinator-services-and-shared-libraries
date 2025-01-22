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

variable "environment_prefix" {
  type     = string
  nullable = false
}

variable "cname_prefix" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = true
}

variable "aws_region" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "aws_account_id" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "container_repo_name" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "ec2_instance_type" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "pbs_budget_keys_dynamodb_table_name" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "pbs_partition_lock_dynamodb_table_name" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "pbs_s3_bucket_name" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "pbs_alternate_domain_record_cname" {
  type     = string
  nullable = true
}

variable "enable_alternate_pbs_domain" {
  type     = bool
  default  = false
  nullable = false
}

variable "alarm_notification_email" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "auth_api_gateway_id" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "auth_api_gateway_base_url" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "beanstalk_app_s3_bucket_name" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "beanstalk_app_version" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "pbs_container_port" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "pbs_container_health_service_port" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "root_volume_size_gb" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "enable_alarms" {
  description = "DEPRECATED"
  type        = bool
  default     = false
  nullable    = false
}

variable "enable_domain_management" {
  type     = bool
  default  = false
  nullable = false
}

variable "parent_domain_name" {
  description = "Custom domain name that redirects to the PBS environment"
  type        = string
}

variable "service_subdomain" {
  description = "Subdomain to use with the parent_domain_name to designate the PBS endpoint."
  type        = string
  default     = "mp-pbs"
}

variable "domain_hosted_zone_id" {
  description = "Hosted zone for route53 record"
  type        = string
}

variable "pbs_partition_lease_duration_seconds" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "application_environment_variables" {
  description = "DEPRECATED"
  type        = map(string)
  default     = {}
}

variable "enable_imds_v2" {
  description = "DEPRECATED"
  type        = bool
  nullable    = false
  default     = true
}

variable "enable_vpc" {
  description = "DEPRECATED"
  type        = bool
  default     = false
}

variable "vpc_id" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "public_subnet_ids" {
  description = "DEPRECATED"
  type        = list(string)
  default     = []
}

variable "private_subnet_ids" {
  description = "DEPRECATED"
  type        = list(string)
  default     = []
}

variable "vpc_default_sg_id" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "remote_coordinator_aws_account_id" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "solution_stack_name" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "sqs_queue_arn" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "sns_topic_arn" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "autoscaling_min_size" {
  description = "DEPRECATED"
  default     = -1
  type        = number
  nullable    = false
}

variable "autoscaling_max_size" {
  description = "DEPRECATED"
  default     = -1
  type        = number
  nullable    = false
}

variable "pbs_s3_bucket_lb_access_logs_id" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "enable_pbs_lb_access_logs" {
  description = "DEPRECATED"
  default     = false
  type        = bool
}

variable "enable_pbs_domain_record_acme" {
  description = "Enable acme_challenge update for PBS domain record certificate creation"
  type        = bool
  default     = false
}

variable "pbs_domain_record_acme" {
  description = "DNS record data for PBS domain record acme_challenge"
  type        = string
  nullable    = true
}
