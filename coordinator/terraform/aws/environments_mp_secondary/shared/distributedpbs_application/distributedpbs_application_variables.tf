# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
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

variable "aws_region" {
  type     = string
  nullable = false
}

variable "environment" {
  type     = string
  nullable = false

  validation {
    condition     = length(var.environment) <= 18
    error_message = "The max length of the environment name is 18 characters."
  }
}

variable "remote_environment" {
  description = "The name of the remote coordinator's environment."
  type        = string
  nullable    = false

  validation {
    condition     = length(var.remote_environment) <= 18
    error_message = "The max length of the remote environment name is 18 characters."
  }
}

variable "cname_prefix" {
  description = "The value to use as the benstalk URL prefix. e.g. if value is ABCD then URL is ABCD.us-east-1.elasticbeanstalk.com. If left blank, a random URL is generated. Defaults to blank."
  type        = string
  default     = ""
  nullable    = true
}

variable "container_repo_name" {
  description = "This is the name of ECR repository. If left blank, the default value used is <environment>-google-scp-pbs-ecr-repository which is expected. Defaults to blank."
  type        = string
  default     = ""
  nullable    = false
}

variable "ec2_instance_type" {
  type     = string
  nullable = false
}

variable "auth_lambda_handler_path" {
  description = <<-EOT
        Path to the python auth lambda handler zip.

        When deployed with multiparty_coordinator_tar.tgz, this is extracted
        at "dist/pbs_auth_handler_lambda.zip".

        When developing within the scp project itself, it is found in
        "//bazel-bin/python/privacybudget/aws/pbs_auth_handler/pbs_auth_handler_lambda.zip"
      EOT
  type        = string
  default     = "../../../dist/pbs_auth_handler_lambda.zip"
}

variable "beanstalk_app_version" {
  description = "The application version. This value must match the PBS container image version tag."
  type        = string
  nullable    = false
}

variable "journal_s3_bucket_force_destroy" {
  description = "Whether to destroy the bucket even if it is not empty."
  type        = bool
  default     = false
  nullable    = false
}

variable "budget_table_read_capacity" {
  type     = number
  default  = 10
  nullable = false
}

variable "budget_table_write_capacity" {
  type     = number
  default  = 10
  nullable = false
}

variable "partition_lock_table_read_capacity" {
  type     = number
  default  = 20
  nullable = false
}

variable "partition_lock_table_write_capacity" {
  type     = number
  default  = 20
  nullable = false
}

variable "beanstalk_app_bucket_force_destroy" {
  description = "Whether to destroy the bucket even if it is not empty."
  type        = bool
  default     = false
  nullable    = false
}

variable "root_volume_size_gb" {
  description = "The size of the EC2 storage in GB"
  type        = string
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
  default     = ""
}

variable "service_subdomain" {
  description = "Subdomain to use with the parent_domain_name to designate the PBS endpoint."
  type        = string
  default     = "mp-pbs"
}

variable "application_environment_variables" {
  description = "Environment variables to be set for the application running on this environment."
  type        = map(string)
  default     = {}
}

variable "enable_vpc" {
  description = "Whether to enable the creation and use of a VPC."
  type        = bool
  default     = false
}

variable "vpc_cidr" {
  description = "VPC CIDR range for coordinator VPC."
  type        = string
  default     = "10.0.0.0/16"
}

variable "availability_zone_replicas" {
  description = "Number of subnet groups to create over additional availability zones in the region (up to 4 for /16, if supported by AWS region)."
  type        = number
  default     = 3
}

variable "auth_table_read_initial_capacity" {
  type     = number
  default  = 100
  nullable = false
}

variable "auth_table_read_max_capacity" {
  type     = number
  default  = 2500
  nullable = false
}

variable "auth_table_read_scale_utilization" {
  type     = number
  default  = 70
  nullable = false
}

variable "auth_table_write_initial_capacity" {
  type     = number
  default  = 100
  nullable = false
}

variable "auth_table_write_max_capacity" {
  type     = number
  default  = 2500
  nullable = false
}

variable "auth_table_write_scale_utilization" {
  type     = number
  default  = 70
  nullable = false
}

variable "remote_coordinator_aws_account_id" {
  description = <<-EOT
        The account ID of the remote coordinator.
        If left blank, the same account of this deployment is used.
      EOT
  type        = string
  default     = ""
  nullable    = false
}

variable "auth_table_enable_point_in_time_recovery" {
  description = "Whether to enable point in time recovery for the auth DynamoDB table."
  type        = bool
  default     = true
}

variable "budget_table_enable_point_in_time_recovery" {
  description = "Whether to enable point in time recovery for the budget-keys DynamoDB table."
  type        = bool
  default     = true
}

variable "partition_lock_table_enable_point_in_time_recovery" {
  description = "Whether to enable point in time recovery for the partition lock DynamoDB table."
  type        = bool
  default     = true
}

variable "beanstalk_solution_stack_name" {
  description = <<-EOT
      The beanstalk solution version. All versions are listed here:
      https://docs.aws.amazon.com/elasticbeanstalk/latest/platforms/platform-history-docker.html
      If no version has been used before, then the latest verison must be used.
  EOT
  type        = string
  nullable    = false
  # Default to empty so it can be overwritten, but we still use the default internal version
  default = ""

  validation {
    # Match any version of the solution stack that uses AL2 running Docker
    condition     = length(var.beanstalk_solution_stack_name) == 0 || length(regexall("64bit Amazon Linux 2 (.*?) running Docker", var.beanstalk_solution_stack_name)) > 0
    error_message = "The solution stack name must use AL2 running Docker"
  }
}

variable "autoscaling_min_size" {
  description = "The minimum number of PBS instances to use for the autoscaling group."
  type        = number
  nullable    = false
  default     = 3
}

variable "autoscaling_max_size" {
  description = "The maximum number of PBS instances to use for the autoscaling group."
  type        = number
  nullable    = false
  default     = 3
}

variable "enable_vpc_flow_logs" {
  description = "Whether to enable VPC flow logs that end up in a cloudwatch log group."
  type        = bool
  default     = false
}

variable "vpc_flow_logs_traffic_type" {
  description = "The traffic type to log. Either ACCEPT, REJECT or ALL."
  type        = string
  default     = "ALL"
}

variable "vpc_flow_logs_retention_in_days" {
  description = "The number of days to keep the flow logs in CloudWatch."
  type        = number
  default     = 90
}

variable "pbs_aws_lb_arn" {
  description = "Elastic Load Balancing account ARN to grant ELB access log policy permission."
  type        = map(string)
  default = {
    "us-east-1" = "127311923021"
    "us-east-2" = "033677994240"
    "us-west-1" = "027434742980"
    "us-west-2" = "797873946194"
  }
}

variable "enable_pbs_lb_access_logs" {
  description = "Wheather to enable PBS ELB access logs."
  type        = bool
  default     = false
}

################################################################################
# PBS Alarm Variables.
################################################################################

variable "alarms_enabled" {
  description = "Enable alarms for PBS services."
  type        = bool
}

variable "alarm_notification_email" {
  description = "Email to receive alarms for PBS service."
  type        = string
}

variable "sns_topic_arn" {
  description = "SNS topic ARN to forward alerts to"
  type        = string
  default     = ""
}

variable "sqs_queue_arn" {
  description = "SQS queue ARN to forward alerts to"
  type        = string
  default     = ""
}

variable "custom_alarm_label" {
  description = "Add any string to the label to help filtering, allowed chars (a-zA-Z_-) max 30 chars"
  type        = string
  default     = ""
}

variable "cloudwatch_logging_retention_days" {
  description = "Number of days to keep logs in Cloudwatch."
  type        = number
  default     = 30
}

variable "pbs_cloudwatch_log_group_name" {
  description = "Cloudwatch log group name to create alerts for (logs with ERROR in the name)"
  type        = string
}

variable "pbs_alarm_eval_period_sec" {
  description = "Amount of time (in seconds) for alarm evaluation. Example: '60'."
  type        = string
  default     = "60"
}

variable "pbs_error_log_log_corrupted_threshold" {
  description = "Alarming threshold for the count of logs containing ERROR - metric_filter_log_corrupted. Example: '0'."
  type        = string
  default     = "0"
}

variable "pbs_error_log_missing_transaction_threshold" {
  description = "Alarming threshold for the count of logs containing ERROR - metric_filter_missing_transaction. Example: '0'."
  type        = string
  default     = "0"
}

variable "pbs_error_log_checkpointing_threshold" {
  description = "Alarming threshold for the count of logs containing ERROR - metric_filter_checkpointing. Example: '0'."
  type        = string
  default     = "0"
}

variable "pbs_error_log_database_read_threshold" {
  description = "Alarming threshold for the count of logs containing ERROR - metric_filter_database_read. Example: '0'."
  type        = string
  default     = "0"
}

variable "pbs_error_log_database_update_threshold" {
  description = "Alarming threshold for the count of logs containing ERROR - metric_filter_database_update. Example: '0'."
  type        = string
  default     = "0"
}

variable "pbs_error_log_missing_component_id_threshold" {
  description = "Alarming threshold for the count of logs containing ERROR - metric_filter_missing_component_id. Example: '0'."
  type        = string
  default     = "0"
}

variable "pbs_error_log_handle_journal_threshold" {
  description = "Alarming threshold for the count of logs containing ERROR - metric_filter_handle_journal. Example: '0'."
  type        = string
  default     = "0"
}

variable "pbs_elb_error_ratio_4xx_threshold" {
  description = "ELB 4xx error ratio rate greater than this to send alarm. Value should be a percentage. Example: 10% is '10.0'."
  type        = string
  default     = "5.0"
}

variable "pbs_elb_error_ratio_5xx_threshold" {
  description = "ELB 5xx error ratio rate greater than this to send alarm. Value should be a percentage. Example: 10% is '10.0'."
  type        = string
  default     = "5.0"
}

variable "budget_key_table_read_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of budget key table read processing unit"
  type        = string
  default     = "0.99"
}

variable "budget_key_table_write_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of budget key table write processing unit"
  type        = string
  default     = "0.99"

}

variable "reporting_origin_table_read_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of reporting origin table read processing unit"
  type        = string
  default     = "0.99"
}

variable "reporting_origin_table_write_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of reporting origin table write processing unit"
  type        = string
  default     = "0.99"
}

variable "pbs_authorization_v2_table_read_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of PBS authorization V2 table read processing unit"
  type        = string
  default     = "0.99"
}

variable "pbs_authorization_v2_table_write_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of PBS authorization V2 table write processing unit"
  type        = string
  default     = "0.99"
}

variable "partition_lock_table_read_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of partition lock table read processing unit"
  type        = string
  default     = "0.99"
}

variable "partition_lock_table_write_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of partition lock table write processing unit"
  type        = string
  default     = "0.99"
}

################################################################################
# PBS Dashboard Variables.
################################################################################

variable "privacy_budget_dashboard_time_period_seconds" {
  description = "Time period that acts as a window for dashboard metrics. Measured in seconds."
  type        = number
  default     = 60
}
