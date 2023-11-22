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

module "distributedpbs_application" {
  source                            = "../../../applications/distributedpbs_application"
  aws_region                        = var.aws_region
  environment                       = var.environment
  cname_prefix                      = var.cname_prefix
  container_repo_name               = var.container_repo_name
  ec2_instance_type                 = var.ec2_instance_type
  auth_lambda_handler_path          = var.auth_lambda_handler_path
  beanstalk_app_version             = var.beanstalk_app_version
  root_volume_size_gb               = var.root_volume_size_gb
  remote_coordinator_aws_account_id = var.remote_coordinator_aws_account_id
  remote_environment                = var.remote_environment
  autoscaling_min_size              = var.autoscaling_min_size
  autoscaling_max_size              = var.autoscaling_max_size
  pbs_aws_lb_arn                    = var.pbs_aws_lb_arn
  enable_pbs_lb_access_logs         = var.enable_pbs_lb_access_logs

  auth_table_read_initial_capacity   = var.auth_table_read_initial_capacity
  auth_table_read_max_capacity       = var.auth_table_read_max_capacity
  auth_table_read_scale_utilization  = var.auth_table_read_scale_utilization
  auth_table_write_initial_capacity  = var.auth_table_write_initial_capacity
  auth_table_write_max_capacity      = var.auth_table_write_max_capacity
  auth_table_write_scale_utilization = var.auth_table_write_scale_utilization

  enable_domain_management = var.enable_domain_management
  parent_domain_name       = var.parent_domain_name
  service_subdomain        = var.service_subdomain

  journal_s3_bucket_force_destroy     = var.journal_s3_bucket_force_destroy
  budget_table_read_capacity          = var.budget_table_read_capacity
  budget_table_write_capacity         = var.budget_table_write_capacity
  partition_lock_table_read_capacity  = var.partition_lock_table_read_capacity
  partition_lock_table_write_capacity = var.partition_lock_table_write_capacity
  beanstalk_app_bucket_force_destroy  = var.beanstalk_app_bucket_force_destroy
  application_environment_variables   = var.application_environment_variables

  enable_vpc                 = var.enable_vpc
  vpc_cidr                   = var.vpc_cidr
  availability_zone_replicas = var.availability_zone_replicas

  auth_table_enable_point_in_time_recovery           = var.auth_table_enable_point_in_time_recovery
  budget_table_enable_point_in_time_recovery         = var.budget_table_enable_point_in_time_recovery
  partition_lock_table_enable_point_in_time_recovery = var.partition_lock_table_enable_point_in_time_recovery

  beanstalk_solution_stack_name = var.beanstalk_solution_stack_name

  alarms_enabled                               = var.alarms_enabled
  alarm_notification_email                     = var.alarm_notification_email
  sns_topic_arn                                = var.sns_topic_arn
  sqs_queue_arn                                = var.sqs_queue_arn
  cloudwatch_logging_retention_days            = var.cloudwatch_logging_retention_days
  pbs_cloudwatch_log_group_name                = var.pbs_cloudwatch_log_group_name
  pbs_alarm_eval_period_sec                    = var.pbs_alarm_eval_period_sec
  pbs_error_log_log_corrupted_threshold        = var.pbs_error_log_log_corrupted_threshold
  pbs_error_log_missing_transaction_threshold  = var.pbs_error_log_missing_transaction_threshold
  pbs_error_log_checkpointing_threshold        = var.pbs_error_log_checkpointing_threshold
  pbs_error_log_database_read_threshold        = var.pbs_error_log_database_read_threshold
  pbs_error_log_database_update_threshold      = var.pbs_error_log_database_update_threshold
  pbs_error_log_missing_component_id_threshold = var.pbs_error_log_missing_component_id_threshold
  pbs_error_log_handle_journal_threshold       = var.pbs_error_log_handle_journal_threshold

  pbs_elb_error_ratio_4xx_threshold = var.pbs_elb_error_ratio_4xx_threshold
  pbs_elb_error_ratio_5xx_threshold = var.pbs_elb_error_ratio_5xx_threshold

  partition_lock_table_read_capacity_alarm_ratio_threshold    = var.partition_lock_table_read_capacity_alarm_ratio_threshold
  partition_lock_table_write_capacity_alarm_ratio_threshold   = var.partition_lock_table_write_capacity_alarm_ratio_threshold
  budget_key_table_read_capacity_alarm_ratio_threshold        = var.budget_key_table_read_capacity_alarm_ratio_threshold
  budget_key_table_write_capacity_alarm_ratio_threshold       = var.budget_key_table_write_capacity_alarm_ratio_threshold
  reporting_origin_table_read_capacity_alarm_ratio_threshold  = var.reporting_origin_table_read_capacity_alarm_ratio_threshold
  reporting_origin_table_write_capacity_alarm_ratio_threshold = var.reporting_origin_table_write_capacity_alarm_ratio_threshold

  enable_vpc_flow_logs            = var.enable_vpc && var.enable_vpc_flow_logs
  vcp_flow_logs_traffic_type      = var.vpc_flow_logs_traffic_type
  vpc_flow_logs_retention_in_days = var.vpc_flow_logs_retention_in_days

  privacy_budget_dashboard_time_period_seconds                    = var.privacy_budget_dashboard_time_period_seconds
  custom_alarm_label                                              = var.custom_alarm_label
  pbs_authorization_v2_table_read_capacity_alarm_ratio_threshold  = var.pbs_authorization_v2_table_read_capacity_alarm_ratio_threshold
  pbs_authorization_v2_table_write_capacity_alarm_ratio_threshold = var.pbs_authorization_v2_table_write_capacity_alarm_ratio_threshold
}
