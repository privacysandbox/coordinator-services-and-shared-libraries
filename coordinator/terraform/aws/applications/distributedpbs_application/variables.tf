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
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = true
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

variable "beanstalk_app_version" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
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
  description = "DEPRECATED"
  type        = bool
  default     = false
  nullable    = false
}

variable "budget_table_read_capacity" {
  description = "DEPRECATED"
  type        = number
  default     = 10
  nullable    = false
}

variable "budget_table_write_capacity" {
  description = "DEPRECATED"
  type        = number
  default     = 10
  nullable    = false
}

variable "partition_lock_table_read_capacity" {
  description = "DEPRECATED"
  type        = number
  default     = 20
  nullable    = false
}

variable "partition_lock_table_write_capacity" {
  description = "DEPRECATED"
  type        = number
  default     = 20
  nullable    = false
}

variable "beanstalk_app_bucket_force_destroy" {
  description = "DEPRECATED"
  type        = bool
  default     = false
  nullable    = false
}

variable "root_volume_size_gb" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
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
  description = "DEPRECATED"
  type        = map(string)
  default     = {}
}

variable "enable_imds_v2" {
  description = "DEPRECATED"
  default     = false
  type        = bool
  nullable    = false
}

variable "enable_vpc" {
  description = "DEPRECATED"
  type        = bool
  default     = false
}

variable "vpc_cidr" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "availability_zone_replicas" {
  description = "DEPRECATED"
  type        = number
  default     = 3
}

variable "auth_table_read_initial_capacity" {
  description = "DEPRECATED"
  type        = number
  default     = 100
  nullable    = false
}

variable "auth_table_read_max_capacity" {
  description = "DEPRECATED"
  type        = number
  default     = 2500
  nullable    = false
}

variable "auth_table_read_scale_utilization" {
  description = "DEPRECATED"
  type        = number
  default     = 70
  nullable    = false
}

variable "auth_table_write_initial_capacity" {
  description = "DEPRECATED"
  type        = number
  default     = 100
  nullable    = false
}

variable "auth_table_write_max_capacity" {
  description = "DEPRECATED"
  type        = number
  default     = 2500
  nullable    = false
}

variable "auth_table_write_scale_utilization" {
  description = "DEPRECATED"
  type        = number
  default     = 70
  nullable    = false
}

variable "remote_coordinator_aws_account_id" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "remote_environment" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
  nullable    = false
}

variable "auth_table_enable_point_in_time_recovery" {
  type = bool
}

variable "budget_table_enable_point_in_time_recovery" {
  description = "DEPRECATED"
  default     = true
  type        = bool
}

variable "partition_lock_table_enable_point_in_time_recovery" {
  description = "DEPRECATED"
  default     = true
  type        = bool
}

variable "beanstalk_solution_stack_name" {
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

variable "pbs_aws_lb_arn" {
  description = "DEPRECATED"
  default     = {}
  type        = map(string)
}

variable "enable_pbs_lb_access_logs" {
  description = "DEPRECATED"
  default     = false
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
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_error_log_log_corrupted_threshold" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_error_log_missing_transaction_eval_periods" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_error_log_missing_transaction_threshold" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_error_log_checkpointing_eval_periods" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_error_log_checkpointing_threshold" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_error_log_database_read_eval_periods" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_error_log_database_read_threshold" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_error_log_database_update_eval_periods" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}
variable "pbs_error_log_database_update_threshold" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_error_log_missing_component_id_eval_periods" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_error_log_missing_component_id_threshold" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_error_log_handle_journal_eval_periods" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_error_log_handle_journal_threshold" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

################################################################################
# ELB based Alarm Variables.
################################################################################

variable "pbs_elb_error_ratio_4xx_eval_periods" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_elb_error_ratio_4xx_threshold" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_elb_error_ratio_4xx_high_eval_periods" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_elb_error_ratio_4xx_high_threshold" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_elb_error_ratio_5xx_eval_periods" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_elb_error_ratio_5xx_threshold" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

################################################################################
# Dynamo Alarm Variables.
################################################################################

variable "budget_key_table_read_capacity_alarm_ratio_eval_periods" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "budget_key_table_read_capacity_alarm_ratio_threshold" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "budget_key_table_write_capacity_alarm_ratio_eval_periods" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "budget_key_table_write_capacity_alarm_ratio_threshold" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_authorization_v2_table_read_capacity_alarm_ratio_eval_periods" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_authorization_v2_table_read_capacity_alarm_ratio_threshold" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_authorization_v2_table_write_capacity_alarm_ratio_eval_periods" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "pbs_authorization_v2_table_write_capacity_alarm_ratio_threshold" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "partition_lock_table_read_capacity_alarm_ratio_eval_periods" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "partition_lock_table_read_capacity_alarm_ratio_threshold" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "partition_lock_table_write_capacity_alarm_ratio_eval_periods" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "partition_lock_table_write_capacity_alarm_ratio_threshold" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
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
  description = "DEPRECATED"
  default     = false
  type        = bool
}

variable "vcp_flow_logs_traffic_type" {
  description = "DEPRECATED"
  default     = "DEPRECATED"
  type        = string
}

variable "vpc_flow_logs_retention_in_days" {
  description = "DEPRECATED"
  default     = -1
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
