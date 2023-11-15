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

########################
# DO NOT EDIT MANUALLY #
########################

# This file is meant to be shared across all environments (either copied or
# symlinked). In order to make the upgrade process easier, this file should not
# be modified for environment-specific customization.

locals {
  # Fall-back to now-deprecated "primary_region"
  region = var.region == "" ? var.primary_region : var.region
}

module "multipartykeyhosting_secondary" {
  source = "../../../applications/multipartykeyhosting_secondary"

  environment = var.environment
  region      = local.region

  allowed_peer_coordinator_principals      = var.allowed_peer_coordinator_principals
  keygeneration_attestation_condition_keys = var.keygeneration_attestation_condition_keys

  create_encryption_key_jar   = var.create_encryption_key_jar
  encryption_key_service_jar  = var.encryption_key_service_jar
  enable_public_key_signature = var.enable_public_key_signature

  enable_domain_management         = var.enable_domain_management
  parent_domain_name               = var.parent_domain_name
  encryption_key_service_subdomain = var.encryption_key_service_subdomain

  api_env_stage_name                   = var.api_env_stage_name
  api_version                          = var.api_version
  keydb_table_name                     = var.keydb_table_name
  enable_dynamo_point_in_time_recovery = var.enable_dynamo_point_in_time_recovery
  enable_keydb_ttl                     = var.enable_keydb_ttl

  keydb_read_capacity                       = var.keydb_read_capacity
  keydb_autoscaling_read_max_capacity       = var.keydb_autoscaling_read_max_capacity
  keydb_autoscaling_read_target_percentage  = var.keydb_autoscaling_read_target_percentage
  keydb_write_capacity                      = var.keydb_write_capacity
  keydb_autoscaling_write_max_capacity      = var.keydb_autoscaling_write_max_capacity
  keydb_autoscaling_write_target_percentage = var.keydb_autoscaling_write_target_percentage

  alarms_enabled                    = var.alarms_enabled
  alarm_notification_email          = var.alarm_notification_email
  sns_topic_arn                     = var.sns_topic_arn
  sqs_queue_arn                     = var.sqs_queue_arn
  cloudwatch_logging_retention_days = var.cloudwatch_logging_retention_days

  mpkhs_alarm_eval_period_sec            = var.mpkhs_alarm_eval_period_sec
  mpkhs_lambda_error_threshold           = var.mpkhs_lambda_error_threshold
  mpkhs_lambda_error_log_threshold       = var.mpkhs_lambda_error_log_threshold
  mpkhs_lambda_max_duration_threshold    = var.mpkhs_lambda_max_duration_threshold
  mpkhs_api_gw_max_latency_ms            = var.mpkhs_api_gw_max_latency_ms
  mpkhs_api_gw_5xx_threshold             = var.mpkhs_api_gw_5xx_threshold
  mpkhs_api_gw_error_ratio_4xx_threshold = var.mpkhs_api_gw_error_ratio_4xx_threshold
  mpkhs_api_gw_error_ratio_5xx_threshold = var.mpkhs_api_gw_error_ratio_5xx_threshold

  get_encryption_key_lambda_ps_client_shim_enabled = var.get_encryption_key_lambda_ps_client_shim_enabled

  availability_zone_replicas      = var.availability_zone_replicas
  enable_vpc                      = var.enable_vpc
  vpc_cidr                        = var.vpc_cidr
  vpc_default_tags                = var.vpc_default_tags
  enable_vpc_flow_logs            = var.enable_vpc && var.enable_vpc_flow_logs
  vpc_flow_logs_traffic_type      = var.vpc_flow_logs_traffic_type
  vpc_flow_logs_retention_in_days = var.vpc_flow_logs_retention_in_days

  unified_keyhosting_dashboard_time_period_seconds = var.unified_keyhosting_dashboard_time_period_seconds
  custom_alarm_label                               = var.custom_alarm_label
}
