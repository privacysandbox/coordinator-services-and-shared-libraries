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

module "pbsprober" {
  source        = "../../../applications/distributedpbs_prober"
  environment   = var.environment
  aws_region    = var.aws_region
  pbsprober_jar = var.pbsprober_jar

  coordinator_a_privacy_budgeting_endpoint           = var.coordinator_a_privacy_budgeting_endpoint
  coordinator_a_privacy_budget_service_auth_endpoint = var.coordinator_a_privacy_budget_service_auth_endpoint
  coordinator_a_role_arn                             = var.coordinator_a_role_arn
  coordinator_a_region                               = var.coordinator_a_region
  coordinator_b_privacy_budgeting_endpoint           = var.coordinator_b_privacy_budgeting_endpoint
  coordinator_b_privacy_budget_service_auth_endpoint = var.coordinator_b_privacy_budget_service_auth_endpoint
  coordinator_b_role_arn                             = var.coordinator_b_role_arn
  coordinator_b_region                               = var.coordinator_b_region
  reporting_origin                                   = var.reporting_origin

  alarms_enabled            = var.alarms_enabled
  alarms_notification_email = var.alarms_notification_email
  alarms_sns_topic_arn      = var.alarms_sns_topic_arn
  alarms_sqs_queue_arn      = var.alarms_sqs_queue_arn
  custom_alarm_label        = var.custom_alarm_label
}
