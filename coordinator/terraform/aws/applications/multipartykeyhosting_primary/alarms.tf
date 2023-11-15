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
  source = "../../monitoring/common/mp_api_gateway_alarms"

  environment     = var.environment
  api_name        = local.keyhosting_api_gateway_alarm_label
  api_gateway_id  = module.apigateway.api_gateway_id
  sns_topic_arn   = var.primary_region_sns_topic_arn == "" ? aws_sns_topic.mpkhs[0].arn : var.primary_region_sns_topic_arn
  eval_period_sec = var.mpkhs_alarm_eval_period_sec

  #Alarms
  error_ratio_4xx_threshold = var.mpkhs_api_gw_error_ratio_4xx_threshold
  error_ratio_5xx_threshold = var.mpkhs_api_gw_error_ratio_5xx_threshold
  custom_alarm_label        = var.custom_alarm_label
}

module "sqs_queue_alarms" {
  count  = var.alarms_enabled ? 1 : 0
  source = "../../monitoring/common/sqs_queue_alarms"

  alarm_label_sqs_queue               = "KeyRotation"
  sqs_queue_name                      = module.key_job_queue.keyjobqueue_queue_name
  environment                         = var.environment
  sns_topic_arn                       = var.primary_region_sns_topic_arn == "" ? aws_sns_topic.mpkhs[0].arn : var.primary_region_sns_topic_arn
  eval_period_sec                     = var.mpkhs_alarm_eval_period_sec
  total_queue_messages_high_threshold = var.mpkhs_total_queue_messages_high_threshold
  custom_alarm_label                  = var.custom_alarm_label
}
