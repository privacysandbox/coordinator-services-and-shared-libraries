# Copyright 2023 Google LLC
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

provider "aws" {
  region = var.aws_region
}

module "mpkhs_publickey_heartbeat_prober" {
  source      = "../../../monitoring/mpkhs_publickey_heartbeat_prober"
  environment = var.environment
  aws_region  = var.aws_region

  heartbeat_source_zip          = var.heartbeat_source_zip
  heartbeat_timeout_sec         = var.heartbeat_timeout_sec
  heartbeat_schedule_expression = var.heartbeat_schedule_expression

  url_to_probe = var.url_to_probe

  alarms_enabled            = var.alarms_enabled
  alarms_eval_periods       = var.alarms_eval_periods
  alarms_threshold          = var.alarms_threshold
  alarms_eval_period_sec    = var.alarms_eval_period_sec
  alarms_notification_email = var.alarms_notification_email
  alarms_sns_topic_arn      = var.alarms_sns_topic_arn
  alarms_sqs_queue_arn      = var.alarms_sqs_queue_arn
  custom_alarm_label        = var.custom_alarm_label
}
