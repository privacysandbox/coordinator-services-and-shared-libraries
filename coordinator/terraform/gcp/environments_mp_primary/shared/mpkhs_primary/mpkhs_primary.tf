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
  source                      = "../../../applications/multipartykeyhosting_primary"
  environment                 = var.environment
  project_id                  = var.project_id
  primary_region              = var.primary_region
  secondary_region            = var.secondary_region
  allowed_operator_user_group = var.allowed_operator_user_group

  alarms_enabled            = var.alarms_enabled
  alarms_notification_email = var.alarms_notification_email

  public_key_load_balancer_logs_enabled = var.public_key_load_balancer_logs_enabled

  enable_domain_management                  = var.enable_domain_management
  parent_domain_name                        = var.parent_domain_name
  parent_domain_name_project                = var.parent_domain_name_project
  service_subdomain_suffix                  = var.service_subdomain_suffix
  public_key_service_subdomain              = var.public_key_service_subdomain
  public_key_service_alternate_domain_names = var.public_key_service_alternate_domain_names
  encryption_key_service_subdomain          = var.encryption_key_service_subdomain

  get_public_key_service_zip = var.get_public_key_service_zip
  encryption_key_service_zip = var.encryption_key_service_zip

  key_generation_image                          = var.key_generation_image
  key_generation_count                          = var.key_generation_count
  key_generation_cron_schedule                  = var.key_generation_cron_schedule
  key_generation_cron_time_zone                 = var.key_generation_cron_time_zone
  key_generation_validity_in_days               = var.key_generation_validity_in_days
  key_generation_ttl_in_days                    = var.key_generation_ttl_in_days
  key_generation_tee_allowed_sa                 = var.key_generation_tee_allowed_sa
  instance_disk_image                           = var.instance_disk_image
  key_generation_logging_enabled                = var.key_generation_logging_enabled
  key_generation_monitoring_enabled             = var.key_generation_monitoring_enabled
  key_generation_tee_restart_policy             = var.key_generation_tee_restart_policy
  key_generation_alignment_period               = var.key_generation_alignment_period
  key_generation_undelivered_messages_threshold = var.key_generation_undelivered_messages_threshold
  key_gen_instance_force_replace                = var.key_gen_instance_force_replace
  peer_coordinator_kms_key_uri                  = var.peer_coordinator_kms_key_uri
  key_storage_service_base_url                  = var.key_storage_service_base_url
  key_storage_service_cloudfunction_url         = var.key_storage_service_cloudfunction_url
  peer_coordinator_wip_provider                 = var.peer_coordinator_wip_provider
  peer_coordinator_service_account              = var.peer_coordinator_service_account
  key_id_type                                   = var.key_id_type
  application_name                              = var.application_name

  cloudfunction_timeout_seconds                      = var.cloudfunction_timeout_seconds
  get_public_key_cloudfunction_memory_mb             = var.get_public_key_cloudfunction_memory_mb
  get_public_key_cloudfunction_min_instances         = var.get_public_key_cloudfunction_min_instances
  get_public_key_cloudfunction_max_instances         = var.get_public_key_cloudfunction_max_instances
  encryption_key_service_cloudfunction_memory_mb     = var.encryption_key_service_cloudfunction_memory_mb
  encryption_key_service_cloudfunction_min_instances = var.encryption_key_service_cloudfunction_min_instances
  encryption_key_service_cloudfunction_max_instances = var.encryption_key_service_cloudfunction_max_instances

  mpkhs_package_bucket_location = var.mpkhs_package_bucket_location
  spanner_instance_config       = var.spanner_instance_config
  spanner_processing_units      = var.spanner_processing_units
  key_db_retention_period       = var.key_db_retention_period

  enable_get_public_key_cdn                    = var.enable_get_public_key_cdn
  get_public_key_cloud_cdn_default_ttl_seconds = var.get_public_key_cloud_cdn_default_ttl_seconds
  get_public_key_cloud_cdn_max_ttl_seconds     = var.get_public_key_cloud_cdn_max_ttl_seconds

  get_public_key_alarm_eval_period_sec                = var.get_public_key_alarm_eval_period_sec
  get_public_key_alarm_duration_sec                   = var.get_public_key_alarm_duration_sec
  get_public_key_cloudfunction_5xx_threshold          = var.get_public_key_cloudfunction_5xx_threshold
  get_public_key_cloudfunction_error_threshold        = var.get_public_key_cloudfunction_error_threshold
  get_public_key_cloudfunction_max_execution_time_max = var.get_public_key_cloudfunction_max_execution_time_max
  get_public_key_lb_5xx_threshold                     = var.get_public_key_lb_5xx_threshold
  get_public_key_lb_max_latency_ms                    = var.get_public_key_lb_max_latency_ms

  encryptionkeyservice_alarm_eval_period_sec                = var.encryptionkeyservice_alarm_eval_period_sec
  encryptionkeyservice_alarm_duration_sec                   = var.encryptionkeyservice_alarm_duration_sec
  encryptionkeyservice_cloudfunction_5xx_threshold          = var.encryptionkeyservice_cloudfunction_5xx_threshold
  encryptionkeyservice_cloudfunction_error_threshold        = var.encryptionkeyservice_cloudfunction_error_threshold
  encryptionkeyservice_cloudfunction_max_execution_time_max = var.encryptionkeyservice_cloudfunction_max_execution_time_max
  encryptionkeyservice_lb_5xx_threshold                     = var.encryptionkeyservice_lb_5xx_threshold
  encryptionkeyservice_lb_max_latency_ms                    = var.encryptionkeyservice_lb_max_latency_ms
}
