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

module "distributed_pbs_logs_alarms" {
  count  = var.alarms_enabled ? 1 : 0
  source = "../../modules/distributedpbs_alarms"

  environment                                 = var.environment
  sns_topic_arn                               = var.sns_topic_arn == "" ? aws_sns_topic.pbs[0].arn : var.sns_topic_arn
  pbs_cloudwatch_log_group_name               = var.pbs_cloudwatch_log_group_name
  error_log_log_corrupted_eval_periods        = var.pbs_error_log_log_corrupted_eval_periods
  error_log_log_corrupted_threshold           = var.pbs_error_log_log_corrupted_threshold
  error_log_missing_transaction_eval_periods  = var.pbs_error_log_missing_transaction_eval_periods
  error_log_missing_transaction_threshold     = var.pbs_error_log_missing_transaction_threshold
  error_log_checkpointing_eval_periods        = var.pbs_error_log_checkpointing_eval_periods
  error_log_checkpointing_threshold           = var.pbs_error_log_checkpointing_threshold
  error_log_database_read_eval_periods        = var.pbs_error_log_database_read_eval_periods
  error_log_database_read_threshold           = var.pbs_error_log_database_read_threshold
  error_log_database_update_eval_periods      = var.pbs_error_log_database_update_eval_periods
  error_log_database_update_threshold         = var.pbs_error_log_database_update_threshold
  error_log_missing_component_id_eval_periods = var.pbs_error_log_missing_component_id_eval_periods
  error_log_missing_component_id_threshold    = var.pbs_error_log_missing_component_id_threshold
  error_log_handle_journal_eval_periods       = var.pbs_error_log_handle_journal_eval_periods
  error_log_handle_journal_threshold          = var.pbs_error_log_handle_journal_threshold

  loadbalancer                      = regex(local.elb_regex, module.beanstalk_environment.elb_loadbalancers[0])
  error_ratio_4xx_eval_periods      = var.pbs_elb_error_ratio_4xx_eval_periods
  error_ratio_4xx_threshold         = var.pbs_elb_error_ratio_4xx_threshold
  error_ratio_4xx_high_eval_periods = var.pbs_elb_error_ratio_4xx_high_eval_periods
  error_ratio_4xx_high_threshold    = var.pbs_elb_error_ratio_4xx_high_threshold
  error_ratio_5xx_eval_periods      = var.pbs_elb_error_ratio_5xx_eval_periods
  error_ratio_5xx_threshold         = var.pbs_elb_error_ratio_5xx_threshold

  partition_lock_table_name = module.storage.partition_lock_dynamo_db_table_name
  budget_key_table_name     = module.storage.budget_keys_dynamo_db_table_name

  partition_lock_table_read_capacity_alarm_ratio_eval_periods  = var.partition_lock_table_read_capacity_alarm_ratio_eval_periods
  partition_lock_table_read_capacity_alarm_ratio_threshold     = var.partition_lock_table_read_capacity_alarm_ratio_threshold
  partition_lock_table_write_capacity_alarm_ratio_eval_periods = var.partition_lock_table_write_capacity_alarm_ratio_eval_periods
  partition_lock_table_write_capacity_alarm_ratio_threshold    = var.partition_lock_table_write_capacity_alarm_ratio_threshold
  budget_key_table_read_capacity_alarm_ratio_eval_periods      = var.budget_key_table_read_capacity_alarm_ratio_eval_periods
  budget_key_table_read_capacity_alarm_ratio_threshold         = var.budget_key_table_read_capacity_alarm_ratio_threshold
  budget_key_table_write_capacity_alarm_ratio_eval_periods     = var.budget_key_table_write_capacity_alarm_ratio_eval_periods
  budget_key_table_write_capacity_alarm_ratio_threshold        = var.budget_key_table_write_capacity_alarm_ratio_threshold

  partition_lock_table_read_capacity                                 = var.partition_lock_table_read_capacity
  partition_lock_table_write_capacity                                = var.partition_lock_table_write_capacity
  budget_table_read_capacity                                         = var.budget_table_read_capacity
  budget_table_write_capacity                                        = var.budget_table_write_capacity
  auth_table_read_max_capacity                                       = var.auth_table_read_max_capacity
  auth_table_write_max_capacity                                      = var.auth_table_write_max_capacity
  custom_alarm_label                                                 = var.custom_alarm_label
  pbs_authorization_v2_table_name                                    = module.auth_db.authorization_dynamo_db_table_v2_name
  pbs_authorization_v2_table_read_capacity_alarm_ratio_eval_periods  = var.pbs_authorization_v2_table_read_capacity_alarm_ratio_eval_periods
  pbs_authorization_v2_table_read_capacity_alarm_ratio_threshold     = var.pbs_authorization_v2_table_read_capacity_alarm_ratio_threshold
  pbs_authorization_v2_table_read_max_capacity                       = var.auth_table_read_max_capacity
  pbs_authorization_v2_table_write_capacity_alarm_ratio_eval_periods = var.pbs_authorization_v2_table_write_capacity_alarm_ratio_eval_periods
  pbs_authorization_v2_table_write_capacity_alarm_ratio_threshold    = var.pbs_authorization_v2_table_write_capacity_alarm_ratio_threshold
  pbs_authorization_v2_table_write_max_capacity                      = var.auth_table_write_max_capacity
}

module "pbs_auth_api_gateway_alarms" {
  count  = var.alarms_enabled ? 1 : 0
  source = "../../monitoring/common/api_gateway_alarms"

  environment    = var.environment
  api_name       = "PBSAuth"
  api_gateway_id = module.auth_service.api_gateway_id
  sns_topic_arn  = var.sns_topic_arn == "" ? aws_sns_topic.pbs[0].arn : var.sns_topic_arn

  max_latency_eval_periods     = var.pbs_auth_api_gw_max_latency_eval_periods
  max_latency_ms_threshold     = var.pbs_auth_api_gw_max_latency_ms_threshold
  error_ratio_4xx_eval_periods = var.pbs_auth_api_gw_4xx_eval_periods
  error_ratio_4xx_threshold    = var.pbs_auth_api_gw_error_ratio_4xx_threshold
  error_ratio_5xx_eval_periods = var.pbs_auth_api_gw_5xx_eval_periods
  error_ratio_5xx_threshold    = var.pbs_auth_api_gw_error_ratio_5xx_threshold
  custom_alarm_label           = var.custom_alarm_label
}

module "pbs_auth_lambda_alarms" {
  count                      = var.alarms_enabled ? 1 : 0
  source                     = "../../monitoring/common/lambda_alarms"
  environment                = var.environment
  lambda_function_name       = module.auth_service.lambda_function_name
  cloudwatch_log_group_name  = module.auth_service.lambda_cloudwatch_log_group_name
  sns_topic_arn              = var.sns_topic_arn == "" ? aws_sns_topic.pbs[0].arn : var.sns_topic_arn
  custom_alarm_label         = var.custom_alarm_label
  lambda_function_name_alarm = "PBSAuth"

  execution_error_eval_periods = var.pbs_auth_lambda_execution_error_eval_periods
  execution_error_threshold    = var.pbs_auth_lambda_execution_error_threshold
  error_log_eval_periods       = var.pbs_auth_lambda_error_log_eval_periods
  error_log_threshold          = var.pbs_auth_lambda_error_log_threshold
  max_duration_eval_periods    = var.pbs_auth_lambda_max_duration_eval_periods
  max_duration_threshold_ms    = var.pbs_auth_lambda_max_duration_threshold_ms
}