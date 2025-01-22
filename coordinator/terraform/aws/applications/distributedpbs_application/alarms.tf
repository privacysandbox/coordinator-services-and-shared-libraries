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
