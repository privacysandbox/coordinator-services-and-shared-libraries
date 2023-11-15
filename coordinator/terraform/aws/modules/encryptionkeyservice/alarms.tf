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

module "get_encryption_key_lambda_alarms" {
  count                      = var.alarms_enabled ? 1 : 0
  source                     = "../../monitoring/common/lambda_alarms"
  environment                = var.environment
  lambda_function_name       = aws_lambda_function.get_encryption_key_lambda.function_name
  cloudwatch_log_group_name  = aws_cloudwatch_log_group.get_encryption_key_lambda_cloudwatch.name
  sns_topic_arn              = var.sns_topic_arn
  execution_error_threshold  = var.lambda_error_threshold
  error_log_threshold        = var.lambda_error_log_threshold
  eval_period_sec            = var.alarm_eval_period_sec
  max_duration_threshold_ms  = var.lambda_max_duration_threshold
  custom_alarm_label         = var.custom_alarm_label
  lambda_function_name_alarm = local.get_encryption_key_lambda_alarm_label
}

module "list_recent_encryption_keys_lambda_alarms" {
  count                      = var.alarms_enabled ? 1 : 0
  source                     = "../../monitoring/common/lambda_alarms"
  environment                = var.environment
  lambda_function_name       = aws_lambda_function.list_recent_encryption_keys_lambda.function_name
  cloudwatch_log_group_name  = aws_cloudwatch_log_group.list_recent_encryption_keys_lambda_cloudwatch.name
  sns_topic_arn              = var.sns_topic_arn
  execution_error_threshold  = var.lambda_error_threshold
  error_log_threshold        = var.lambda_error_log_threshold
  eval_period_sec            = var.alarm_eval_period_sec
  max_duration_threshold_ms  = var.lambda_max_duration_threshold
  custom_alarm_label         = var.custom_alarm_label
  lambda_function_name_alarm = local.list_recent_encryption_keys_lambda_alarm_label
}
