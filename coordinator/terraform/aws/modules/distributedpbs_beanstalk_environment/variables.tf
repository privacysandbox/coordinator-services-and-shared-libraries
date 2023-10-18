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
  type     = string
  nullable = true
}

variable "aws_region" {
  type     = string
  nullable = false
}

variable "aws_account_id" {
  type     = string
  nullable = false
}

variable "container_repo_name" {
  type     = string
  nullable = false
}

variable "ec2_instance_type" {
  type     = string
  nullable = false
}

variable "pbs_budget_keys_dynamodb_table_name" {
  type     = string
  nullable = false
}

variable "pbs_partition_lock_dynamodb_table_name" {
  type     = string
  nullable = false
}

variable "pbs_s3_bucket_name" {
  type     = string
  nullable = false
}

variable "alarm_notification_email" {
  type     = string
  nullable = false
}

variable "auth_api_gateway_id" {
  type     = string
  nullable = false
}

variable "auth_api_gateway_base_url" {
  type     = string
  nullable = false
}

variable "beanstalk_app_s3_bucket_name" {
  type     = string
  nullable = false
}

variable "beanstalk_app_version" {
  type     = string
  nullable = false
}

variable "pbs_container_port" {
  description = "The port that the PBS application will run within the container."
  type        = string
  nullable    = false
}

variable "pbs_container_health_service_port" {
  description = "The port that the PBS health service will run within the container."
  type        = string
  nullable    = false
}

variable "root_volume_size_gb" {
  type     = string
  nullable = false
}

variable "enable_alarms" {
  type     = bool
  default  = false
  nullable = false
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
  description = "The partition lease duration in seconds for multi-instance environments."
  type        = string
  nullable    = false
}

variable "application_environment_variables" {
  description = "Environment variables to be set for the application running on this environment."
  type        = map(string)
  default     = {}
}

variable "enable_vpc" {
  description = "Whether to use a VPC for the environment."
  type        = bool
  default     = false
}

variable "vpc_id" {
  description = "The ID of the VPC."
  type        = string
  default     = ""
}

variable "public_subnet_ids" {
  description = "The IDs of the public subnets."
  type        = list(string)
  default     = []
}

variable "private_subnet_ids" {
  description = "The IDs of the private subnets."
  type        = list(string)
  default     = []
}

variable "vpc_default_sg_id" {
  description = "The ID of the VPC default security group."
  type        = string
  default     = ""
}

variable "remote_coordinator_aws_account_id" {
  description = "The account ID of the remote coordinator."
  type        = string
  nullable    = false
}

variable "solution_stack_name" {
  description = <<-EOT
      The beanstalk solution version. All versions are listed here:
      https://docs.aws.amazon.com/elasticbeanstalk/latest/platforms/platform-history-docker.html
      If no version has been used before, then the latest verison must be used.
  EOT
  type        = string
  nullable    = false

  validation {
    # Match any version of the solution stack that uses AL2 running Docker
    condition     = length(var.solution_stack_name) == 0 || length(regexall("64bit Amazon Linux 2 (.*?) running Docker", var.solution_stack_name)) > 0
    error_message = "The solution stack name must use AL2 running Docker"
  }
}

variable "sqs_queue_arn" {
  type     = string
  nullable = false
}

variable "sns_topic_arn" {
  type     = string
  nullable = false
}

variable "autoscaling_min_size" {
  description = "The minimum number of PBS instances to use for the autoscaling group."
  type        = number
  nullable    = false

  validation {
    condition     = var.autoscaling_min_size > 0
    error_message = "The minimum number of PBS instances in the autoscaling group must be at least one."
  }
}

variable "autoscaling_max_size" {
  description = "The maximum number of PBS instances to use for the autoscaling group."
  type        = number
  nullable    = false

  validation {
    condition     = var.autoscaling_max_size > 0
    error_message = "The maximum number of PBS instances in the autoscaling group must be at least one."
  }
}

variable "pbs_s3_bucket_lb_access_logs_id" {
  description = "PBS ELB access log bucket ID."
  type        = string
}

variable "enable_pbs_lb_access_logs" {
  description = "Wheather to enable PBS ELB access logs."
  type        = bool
}
