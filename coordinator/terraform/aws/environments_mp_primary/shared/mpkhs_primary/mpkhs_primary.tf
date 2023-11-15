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

module "multipartykeyhosting_primary" {
  source = "../../../applications/multipartykeyhosting_primary"

  environment      = var.environment
  primary_region   = var.primary_region
  secondary_region = var.secondary_region

  keydb_table_name                          = var.keydb_table_name
  enable_dynamodb_replica                   = var.enable_dynamodb_replica
  enable_dynamo_point_in_time_recovery      = var.enable_dynamo_point_in_time_recovery
  enable_keydb_ttl                          = var.enable_keydb_ttl
  keydb_write_capacity                      = var.keydb_write_capacity
  keydb_read_capacity                       = var.keydb_read_capacity
  keydb_autoscaling_read_max_capacity       = var.keydb_autoscaling_read_max_capacity
  keydb_autoscaling_read_target_percentage  = var.keydb_autoscaling_read_target_percentage
  keydb_autoscaling_write_max_capacity      = var.keydb_autoscaling_write_max_capacity
  keydb_autoscaling_write_target_percentage = var.keydb_autoscaling_write_target_percentage

  key_generation_ami_name_prefix           = var.key_generation_ami_name_prefix
  key_generation_ami_owners                = var.key_generation_ami_owners
  keystorage_api_base_url                  = var.keystorage_api_base_url
  key_generation_frequency_in_hours        = var.key_generation_frequency_in_hours
  key_generation_count                     = var.key_generation_count
  key_generation_validity_in_days          = var.key_generation_validity_in_days
  key_generation_ttl_in_days               = var.key_generation_ttl_in_days
  get_public_key_cache_control_max_seconds = var.get_public_key_cache_control_max_seconds
  min_cloudfront_ttl_seconds               = var.min_cloudfront_ttl_seconds
  max_cloudfront_ttl_seconds               = var.max_cloudfront_ttl_seconds
  default_cloudfront_ttl_seconds           = var.default_cloudfront_ttl_seconds
  coordinator_b_assume_role_arn            = var.coordinator_b_assume_role_arn
  enable_public_key_signature              = var.enable_public_key_signature
  key_id_type                              = var.key_id_type

  public_key_service_jar                                = var.public_key_service_jar
  encryption_key_service_jar                            = var.encryption_key_service_jar
  get_public_key_lambda_memory_mb                       = var.get_public_key_lambda_memory_mb
  get_public_key_lambda_provisioned_concurrency_enabled = var.get_public_key_lambda_provisioned_concurrency_enabled
  get_public_key_lambda_provisioned_concurrency_count   = var.get_public_key_lambda_provisioned_concurrency_count

  get_encryption_key_lambda_ps_client_shim_enabled = var.get_encryption_key_lambda_ps_client_shim_enabled

  enable_domain_management                                = var.enable_domain_management
  parent_domain_name                                      = var.parent_domain_name
  public_key_service_subdomain                            = var.public_key_service_subdomain
  public_key_service_alternate_domain_names               = var.public_key_service_alternate_domain_names
  public_key_service_domain_name_to_domain_hosted_zone_id = var.public_key_service_domain_name_to_domain_hosted_zone_id
  encryption_key_service_subdomain                        = var.encryption_key_service_subdomain
  api_version                                             = var.api_version
  api_env_stage_name                                      = var.api_env_stage_name
  application_name                                        = var.application_name

  alarms_enabled                    = var.alarms_enabled
  alarm_notification_email          = var.alarm_notification_email
  primary_region_sns_topic_arn      = var.primary_region_sns_topic_arn
  secondary_region_sns_topic_arn    = var.secondary_region_sns_topic_arn
  primary_region_sqs_queue_arn      = var.primary_region_sqs_queue_arn
  secondary_region_sqs_queue_arn    = var.secondary_region_sqs_queue_arn
  cloudwatch_logging_retention_days = var.cloudwatch_logging_retention_days

  mpkhs_alarm_eval_period_sec               = var.mpkhs_alarm_eval_period_sec
  mpkhs_lambda_error_threshold              = var.mpkhs_lambda_error_threshold
  mpkhs_lambda_error_log_threshold          = var.mpkhs_lambda_error_log_threshold
  mpkhs_lambda_max_duration_threshold       = var.mpkhs_lambda_max_duration_threshold
  mpkhs_api_gw_max_latency_ms               = var.mpkhs_api_gw_max_latency_ms
  mpkhs_api_gw_5xx_threshold                = var.mpkhs_api_gw_5xx_threshold
  mpkhs_api_gw_error_ratio_4xx_threshold    = var.mpkhs_api_gw_error_ratio_4xx_threshold
  mpkhs_api_gw_error_ratio_5xx_threshold    = var.mpkhs_api_gw_error_ratio_5xx_threshold
  mpkhs_total_queue_messages_high_threshold = var.mpkhs_total_queue_messages_high_threshold

  get_public_key_cloudfront_5xx_threshold            = var.get_public_key_cloudfront_5xx_threshold
  get_public_key_cloudfront_cache_hit_threshold      = var.get_public_key_cloudfront_cache_hit_threshold
  get_public_key_cloudfront_origin_latency_threshold = var.get_public_key_cloudfront_origin_latency_threshold

  availability_zone_replicas      = var.availability_zone_replicas
  enable_vpc                      = var.enable_vpc
  vpc_cidr                        = var.vpc_cidr
  vpc_default_tags                = var.vpc_default_tags
  enable_vpc_flow_logs            = var.enable_vpc && var.enable_vpc_flow_logs
  vpc_flow_logs_traffic_type      = var.vpc_flow_logs_traffic_type
  vpc_flow_logs_retention_in_days = var.vpc_flow_logs_retention_in_days

  unified_keyhosting_dashboard_time_period_seconds = var.unified_keyhosting_dashboard_time_period_seconds
  public_keyhosting_dashboard_time_period_seconds  = var.public_keyhosting_dashboard_time_period_seconds
  custom_alarm_label                               = var.custom_alarm_label
}
