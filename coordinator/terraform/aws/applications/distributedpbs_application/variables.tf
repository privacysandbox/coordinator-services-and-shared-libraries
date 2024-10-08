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

variable "aws_region" {
  type     = string
  nullable = false
}

variable "environment" {
  type     = string
  nullable = false
}

variable "cname_prefix" {
  type     = string
  nullable = true
}

variable "container_repo_name" {
  type     = string
  nullable = false
}

variable "ec2_instance_type" {
  type     = string
  nullable = false
}

variable "beanstalk_app_version" {
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
  type     = string
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

variable "application_environment_variables" {
  description = "Environment variables to be set for the application running on this environment."
  type        = map(string)
  default     = {}
}

variable "enable_imds_v2" {
  description = "Whether to enable IMDSv2 on this environment"
  type        = bool
  nullable    = false
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
  description = "The account ID of the remote coordinator."
  type        = string
  nullable    = false
}

variable "remote_environment" {
  description = "The name of the remote coordinator's environment."
  type        = string
  nullable    = false
}

variable "auth_table_enable_point_in_time_recovery" {
  type = bool
}

variable "budget_table_enable_point_in_time_recovery" {
  type = bool
}

variable "partition_lock_table_enable_point_in_time_recovery" {
  type = bool
}

variable "beanstalk_solution_stack_name" {
  description = <<-EOT
      The beanstalk solution version. All versions are listed here:
      https://docs.aws.amazon.com/elasticbeanstalk/latest/platforms/platform-history-docker.html
      If no version has been used before, then the latest verison must be used.
  EOT
  type        = string
  nullable    = false

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
}

variable "autoscaling_max_size" {
  description = "The maximum number of PBS instances to use for the autoscaling group."
  type        = number
  nullable    = false
}

variable "pbs_aws_lb_arn" {
  description = "Elastic Load Balancing account ARN to grant ELB access log policy permission."
  type        = map(string)
}

variable "enable_pbs_lb_access_logs" {
  description = "Wheather to enable PBS ELB access logs."
  type        = bool
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
}

variable "sqs_queue_arn" {
  description = "SQS queue ARN to forward alerts to"
  type        = string
}

variable "custom_alarm_label" {
  description = "Add any string to the label to help filtering, allowed chars (a-zA-Z_-) max 30 chars"
  type        = string
}

variable "cloudwatch_logging_retention_days" {
  description = "Number of days to keep logs in Cloudwatch."
  type        = number
}

variable "pbs_cloudwatch_log_group_name" {
  description = "Cloudwatch log group name to create alerts for (logs with ERROR in the name)"
  type        = string
}

################################################################################
# Log based Alarm Variables.
################################################################################

variable "pbs_error_log_log_corrupted_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "pbs_error_log_log_corrupted_threshold" {
  description = "Alarming threshold for the count of logs containing ERROR - metric_filter_log_corrupted. Example: '0'."
  type        = string
}

variable "pbs_error_log_missing_transaction_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "pbs_error_log_missing_transaction_threshold" {
  description = "Alarming threshold for the count of logs containing ERROR - metric_filter_missing_transaction. Example: '0'."
  type        = string
}

variable "pbs_error_log_checkpointing_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "pbs_error_log_checkpointing_threshold" {
  description = "Alarming threshold for the count of logs containing ERROR - metric_filter_checkpointing. Example: '0'."
  type        = string
}

variable "pbs_error_log_database_read_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "pbs_error_log_database_read_threshold" {
  description = "Alarming threshold for the count of logs containing ERROR - metric_filter_database_read. Example: '0'."
  type        = string
}

variable "pbs_error_log_database_update_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}
variable "pbs_error_log_database_update_threshold" {
  description = "Alarming threshold for the count of logs containing ERROR - metric_filter_database_update. Example: '0'."
  type        = string
}

variable "pbs_error_log_missing_component_id_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "pbs_error_log_missing_component_id_threshold" {
  description = "Alarming threshold for the count of logs containing ERROR - metric_filter_missing_component_id. Example: '0'."
  type        = string
}

variable "pbs_error_log_handle_journal_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "pbs_error_log_handle_journal_threshold" {
  description = "Alarming threshold for the count of logs containing ERROR - metric_filter_handle_journal. Example: '0'."
  type        = string
}

################################################################################
# ELB based Alarm Variables.
################################################################################

variable "pbs_elb_error_ratio_4xx_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "pbs_elb_error_ratio_4xx_threshold" {
  description = "ELB 4xx error ratio rate greater than this to send alarm. Value should be a percentage. Example: 10% is '10.0'."
  type        = string
}

variable "pbs_elb_error_ratio_4xx_high_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "pbs_elb_error_ratio_4xx_high_threshold" {
  description = "ELB 4xx error ratio rate greater than this to send alarm. Value should be a percentage. Example: 30% is '30.0'."
  type        = string
}

variable "pbs_elb_error_ratio_5xx_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "pbs_elb_error_ratio_5xx_threshold" {
  description = "ELB 5xx error ratio rate greater than this to send alarm. Value should be a percentage. Example: 10% is '10.0'."
  type        = string
}

################################################################################
# Dynamo Alarm Variables.
################################################################################

variable "budget_key_table_read_capacity_alarm_ratio_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "budget_key_table_read_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of budget key table read processing unit"
  type        = string
}

variable "budget_key_table_write_capacity_alarm_ratio_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "budget_key_table_write_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of budget key table write processing unit"
  type        = string
}

variable "pbs_authorization_v2_table_read_capacity_alarm_ratio_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "pbs_authorization_v2_table_read_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of PBS authorization V2 table read processing unit"
  type        = string
}

variable "pbs_authorization_v2_table_write_capacity_alarm_ratio_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "pbs_authorization_v2_table_write_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of PBS authorization V2 table write processing unit"
  type        = string
}

variable "partition_lock_table_read_capacity_alarm_ratio_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "partition_lock_table_read_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of partition lock key table read processing unit"
  type        = string
}

variable "partition_lock_table_write_capacity_alarm_ratio_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "partition_lock_table_write_capacity_alarm_ratio_threshold" {
  description = "The capacity limit of partition lock table write processing unit"
  type        = string
}
################################################################################
# PBS Auth API Gateway Alarm Variables.
################################################################################

variable "pbs_auth_api_gw_max_latency_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "pbs_auth_api_gw_max_latency_ms_threshold" {
  description = "API Gateway max latency to send alarm. Measured in milliseconds. Default 10s because API takes 6-7 seconds on cold start."
  type        = string
  default     = "10000"
}

variable "pbs_auth_api_gw_4xx_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "pbs_auth_api_gw_error_ratio_4xx_threshold" {
  description = "API Gateway 4xx error ratio rate greater than this to send alarm. Must be in decimal form: 10% = 0.10. Example: '0.0'."
  type        = string
}

variable "pbs_auth_api_gw_5xx_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "pbs_auth_api_gw_error_ratio_5xx_threshold" {
  description = "API Gateway 5xx error ratio rate greater than this to send alarm. Must be in decimal form: 10% = 0.10. Example: '0.0'."
  type        = string
}

################################################################################
# PBS Auth Lambda Alarm Variables.
################################################################################

variable "pbs_auth_lambda_execution_error_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "pbs_auth_lambda_execution_error_threshold" {
  description = "Alarming threshold for Lambda execution errors. Must be in decimal form: 10% = 0.10. Example: '0.0'."
  type        = string
}

variable "pbs_auth_lambda_error_log_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "pbs_auth_lambda_error_log_threshold" {
  description = "Alarming threshold for the the count of logs containing ERROR found in the provided cloudwatch_log_group_name. Example: '0'."
  type        = string
}

variable "pbs_auth_lambda_max_duration_eval_periods" {
  description = "Evaluation Periods: is the number of the most recent periods, or data points, to evaluate when determining alarm state. Example: '5'."
  type        = string
}

variable "pbs_auth_lambda_max_duration_threshold_ms" {
  description = "Lambda max duration in ms to send alarm. Useful for timeouts. Example: '9999'."
  type        = string
}

################################################################################
# VPC Variables.
################################################################################

variable "enable_vpc_flow_logs" {
  description = "Whether to enable VPC flow logs that end up in a cloudwatch log group."
  type        = bool
}

variable "vcp_flow_logs_traffic_type" {
  description = "The traffic type to log. Either ACCEPT, REJECT or ALL."
  type        = string
}

variable "vpc_flow_logs_retention_in_days" {
  description = "The number of days to keep the flow logs in CloudWatch."
  type        = number
}

################################################################################
# Dashboard Variables.
################################################################################

variable "privacy_budget_dashboard_time_period_seconds" {
  description = "Time period that acts as a window for dashboard metrics. Measured in seconds."
  type        = number
}

################################################################################
# PBS alternate domain
################################################################################

variable "pbs_alternate_domain_record_cname" {
  type     = string
  nullable = true
}

variable "enable_alternate_pbs_domain" {
  type     = bool
  default  = false
  nullable = false
}