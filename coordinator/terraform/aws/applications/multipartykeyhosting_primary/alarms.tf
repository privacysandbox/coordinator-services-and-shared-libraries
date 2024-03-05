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

module "unified_key_hosting_api_gateway_alarms" {
  count  = var.alarms_enabled ? 1 : 0
  source = "../../monitoring/common/api_gateway_alarms"

  environment    = var.environment
  api_name       = "UnifiedKeyHosting"
  api_gateway_id = module.apigateway.api_gateway_id
  sns_topic_arn  = var.primary_region_sns_topic_arn == "" ? aws_sns_topic.mpkhs[0].arn : var.primary_region_sns_topic_arn

  max_latency_eval_periods     = var.ukh_api_gw_max_latency_eval_periods
  max_latency_ms_threshold     = var.ukh_api_gw_max_latency_ms_threshold
  error_ratio_4xx_eval_periods = var.ukh_api_gw_4xx_eval_periods
  error_ratio_4xx_threshold    = var.ukh_api_gw_error_ratio_4xx_threshold
  error_ratio_5xx_eval_periods = var.ukh_api_gw_5xx_eval_periods
  error_ratio_5xx_threshold    = var.ukh_api_gw_error_ratio_5xx_threshold
  custom_alarm_label           = var.custom_alarm_label
}

module "get_public_key_primary_api_gateway_alarms" {
  count  = var.alarms_enabled ? 1 : 0
  source = "../../monitoring/common/api_gateway_alarms"

  environment    = var.environment
  api_name       = "GetPublicKey"
  api_gateway_id = module.publickeyhostingservice_primary.api_gateway_id
  sns_topic_arn  = var.primary_region_sns_topic_arn == "" ? aws_sns_topic.mpkhs[0].arn : var.primary_region_sns_topic_arn

  max_latency_eval_periods     = var.gpk_api_gw_max_latency_eval_periods
  max_latency_ms_threshold     = var.gpk_api_gw_max_latency_ms_threshold
  error_ratio_4xx_eval_periods = var.gpk_api_gw_4xx_eval_periods
  error_ratio_4xx_threshold    = var.gpk_api_gw_error_ratio_4xx_threshold
  error_ratio_5xx_eval_periods = var.gpk_api_gw_5xx_eval_periods
  error_ratio_5xx_threshold    = var.gpk_api_gw_error_ratio_5xx_threshold
  custom_alarm_label           = var.custom_alarm_label
}

module "get_public_key_secondary_api_gateway_alarms" {
  count  = var.alarms_enabled ? 1 : 0
  source = "../../monitoring/common/api_gateway_alarms"

  providers = {
    aws = aws.secondary
  }

  environment    = var.environment
  api_name       = "GetPublicKey"
  api_gateway_id = module.publickeyhostingservice_secondary.api_gateway_id
  sns_topic_arn  = var.secondary_region_sns_topic_arn == "" ? aws_sns_topic.mpkhs_snstopic_secondary[0].arn : var.secondary_region_sns_topic_arn

  max_latency_eval_periods     = var.gpk_api_gw_max_latency_eval_periods
  max_latency_ms_threshold     = var.gpk_api_gw_max_latency_ms_threshold
  error_ratio_4xx_eval_periods = var.gpk_api_gw_4xx_eval_periods
  error_ratio_4xx_threshold    = var.gpk_api_gw_error_ratio_4xx_threshold
  error_ratio_5xx_eval_periods = var.gpk_api_gw_5xx_eval_periods
  error_ratio_5xx_threshold    = var.gpk_api_gw_error_ratio_5xx_threshold
  custom_alarm_label           = var.custom_alarm_label
}

module "sqs_queue_alarms" {
  count  = var.alarms_enabled ? 1 : 0
  source = "../../monitoring/common/sqs_queue_alarms"

  alarm_label_sqs_queue                  = "KeyRotation"
  sqs_queue_name                         = module.key_job_queue.keyjobqueue_queue_name
  environment                            = var.environment
  sns_topic_arn                          = var.primary_region_sns_topic_arn == "" ? aws_sns_topic.mpkhs[0].arn : var.primary_region_sns_topic_arn
  total_queue_messages_high_eval_periods = var.mpkhs_total_queue_messages_high_eval_periods
  total_queue_messages_high_threshold    = var.mpkhs_total_queue_messages_high_threshold
  custom_alarm_label                     = var.custom_alarm_label
}

module "get_encryption_key_lambda_alarms" {
  count                      = var.alarms_enabled ? 1 : 0
  source                     = "../../monitoring/common/lambda_alarms"
  environment                = var.environment
  lambda_function_name       = module.encryptionkeyservice.get_encryption_key_lambda_function_name
  cloudwatch_log_group_name  = module.encryptionkeyservice.get_encryption_key_lambda_cloudwatch_log_group_name
  sns_topic_arn              = var.primary_region_sns_topic_arn == "" ? aws_sns_topic.mpkhs[0].arn : var.primary_region_sns_topic_arn
  custom_alarm_label         = var.custom_alarm_label
  lambda_function_name_alarm = "GetEncryptionKey"

  execution_error_eval_periods = var.gek_lambda_execution_error_eval_periods
  execution_error_threshold    = var.gek_lambda_execution_error_threshold
  error_log_eval_periods       = var.gek_lambda_error_log_eval_periods
  error_log_threshold          = var.gek_lambda_error_log_threshold
  max_duration_eval_periods    = var.gek_lambda_max_duration_eval_periods
  max_duration_threshold_ms    = var.gek_lambda_max_duration_threshold_ms
}

module "list_recent_encryption_keys_lambda_alarms" {
  count                      = var.alarms_enabled ? 1 : 0
  source                     = "../../monitoring/common/lambda_alarms"
  environment                = var.environment
  lambda_function_name       = module.encryptionkeyservice.list_recent_encryption_keys_lambda_function_name
  cloudwatch_log_group_name  = module.encryptionkeyservice.list_recent_encryption_keys_lambda_cloudwatch_log_group_name
  sns_topic_arn              = var.primary_region_sns_topic_arn == "" ? aws_sns_topic.mpkhs[0].arn : var.primary_region_sns_topic_arn
  custom_alarm_label         = var.custom_alarm_label
  lambda_function_name_alarm = "ListRecentEncryptionKeys"

  execution_error_eval_periods = var.lrek_lambda_execution_error_eval_periods
  execution_error_threshold    = var.lrek_lambda_execution_error_threshold
  error_log_eval_periods       = var.lrek_lambda_error_log_eval_periods
  error_log_threshold          = var.lrek_lambda_error_log_threshold
  max_duration_eval_periods    = var.lrek_lambda_max_duration_eval_periods
  max_duration_threshold_ms    = var.lrek_lambda_max_duration_threshold_ms
}

module "get_public_key_primary_lambda_alarms" {
  count                      = var.alarms_enabled ? 1 : 0
  source                     = "../../monitoring/common/lambda_alarms"
  environment                = var.environment
  lambda_function_name       = module.publickeyhostingservice_primary.get_public_key_lambda_function_name
  cloudwatch_log_group_name  = module.publickeyhostingservice_primary.get_public_key_lambda_cloudwatch_log_group_name
  sns_topic_arn              = var.primary_region_sns_topic_arn == "" ? aws_sns_topic.mpkhs[0].arn : var.primary_region_sns_topic_arn
  custom_alarm_label         = var.custom_alarm_label
  lambda_function_name_alarm = "GetPublicKey"

  execution_error_eval_periods = var.gpk_lambda_execution_error_eval_periods
  execution_error_threshold    = var.gpk_lambda_execution_error_threshold
  error_log_eval_periods       = var.gpk_lambda_error_log_eval_periods
  error_log_threshold          = var.gpk_lambda_error_log_threshold
  max_duration_eval_periods    = var.gpk_lambda_max_duration_eval_periods
  max_duration_threshold_ms    = var.gpk_lambda_max_duration_threshold_ms
}

module "get_public_key_secondary_lambda_alarms" {
  count  = var.alarms_enabled ? 1 : 0
  source = "../../monitoring/common/lambda_alarms"

  providers = {
    aws = aws.secondary
  }

  environment                = var.environment
  lambda_function_name       = module.publickeyhostingservice_secondary.get_public_key_lambda_function_name
  cloudwatch_log_group_name  = module.publickeyhostingservice_secondary.get_public_key_lambda_cloudwatch_log_group_name
  sns_topic_arn              = var.secondary_region_sns_topic_arn == "" ? aws_sns_topic.mpkhs_snstopic_secondary[0].arn : var.secondary_region_sns_topic_arn
  custom_alarm_label         = var.custom_alarm_label
  lambda_function_name_alarm = "GetPublicKey"

  execution_error_eval_periods = var.gpk_lambda_execution_error_eval_periods
  execution_error_threshold    = var.gpk_lambda_execution_error_threshold
  error_log_eval_periods       = var.gpk_lambda_error_log_eval_periods
  error_log_threshold          = var.gpk_lambda_error_log_threshold
  max_duration_eval_periods    = var.gpk_lambda_max_duration_eval_periods
  max_duration_threshold_ms    = var.gpk_lambda_max_duration_threshold_ms
}

module "get_public_key_cloudfront_alarms" {
  count  = var.alarms_enabled ? 1 : 0
  source = "../../monitoring/common/cloudfront_alarms"

  # Cloudfront alarms and ssl certificates must be in us-east-1:
  # https://docs.aws.amazon.com/AmazonCloudFront/latest/DeveloperGuide/programming-cloudwatch-metrics.html
  # https://docs.aws.amazon.com/AmazonCloudFront/latest/DeveloperGuide/cnames-and-https-requirements.html
  providers = {
    aws = aws.useast1
  }

  sns_topic_arn = local.sns_cloudfront_arn == "" ? aws_sns_topic.mpkhs_snstopic_useast1[0].arn : local.sns_cloudfront_arn

  cloudfront_alarm_name_prefix           = "GetPublicKey"
  cloudfront_distribution_id             = module.publickeyhostingcloudfront.cloudfront_id
  cloudfront_5xx_eval_periods            = var.get_public_key_cloudfront_5xx_eval_periods
  cloudfront_5xx_threshold               = var.get_public_key_cloudfront_5xx_threshold
  cloudfront_cache_hit_eval_periods      = var.get_public_key_cloudfront_cache_hit_eval_periods
  cloudfront_cache_hit_threshold         = var.get_public_key_cloudfront_cache_hit_threshold
  cloudfront_origin_latency_eval_periods = var.get_public_key_cloudfront_origin_latency_eval_periods
  cloudfront_origin_latency_threshold    = var.get_public_key_cloudfront_origin_latency_threshold
  custom_alarm_label                     = var.custom_alarm_label
}