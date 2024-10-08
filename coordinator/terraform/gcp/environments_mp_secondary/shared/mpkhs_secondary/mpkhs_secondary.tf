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

module "multipartykeyhosting_secondary" {
  source                                          = "../../../applications/multipartykeyhosting_secondary"
  environment                                     = var.environment
  project_id                                      = var.project_id
  wipp_project_id_override                        = var.wipp_project_id_override
  wip_allowed_service_account_project_id_override = var.wip_allowed_service_account_project_id_override
  primary_region                                  = var.primary_region
  secondary_region                                = var.secondary_region
  allowed_operator_user_group                     = var.allowed_operator_user_group
  enable_audit_log                                = var.enable_audit_log

  alarms_enabled            = var.alarms_enabled
  alarms_notification_email = var.alarms_notification_email

  enable_domain_management         = var.enable_domain_management
  parent_domain_name               = var.parent_domain_name
  parent_domain_name_project       = var.parent_domain_name_project
  service_subdomain_suffix         = var.service_subdomain_suffix
  encryption_key_service_subdomain = var.encryption_key_service_subdomain
  key_storage_service_subdomain    = var.key_storage_service_subdomain

  key_storage_service_zip    = var.key_storage_service_zip
  encryption_key_service_zip = var.encryption_key_service_zip

  cloudfunction_timeout_seconds                      = var.cloudfunction_timeout_seconds
  encryption_key_service_cloudfunction_memory_mb     = var.encryption_key_service_cloudfunction_memory_mb
  encryption_key_service_cloudfunction_min_instances = var.encryption_key_service_cloudfunction_min_instances
  encryption_key_service_cloudfunction_max_instances = var.encryption_key_service_cloudfunction_max_instances
  key_storage_service_cloudfunction_memory_mb        = var.key_storage_service_cloudfunction_memory_mb
  key_storage_service_cloudfunction_min_instances    = var.key_storage_service_cloudfunction_min_instances
  key_storage_service_cloudfunction_max_instances    = var.key_storage_service_cloudfunction_max_instances

  mpkhs_package_bucket_location = var.mpkhs_package_bucket_location
  spanner_instance_config       = var.spanner_instance_config
  spanner_processing_units      = var.spanner_processing_units
  key_db_retention_period       = var.key_db_retention_period

  allowed_wip_iam_principals                   = var.allowed_wip_iam_principals
  allowed_wip_user_group                       = var.allowed_wip_user_group
  enable_attestation                           = var.enable_attestation
  assertion_tee_swname                         = var.assertion_tee_swname
  assertion_tee_support_attributes             = var.assertion_tee_support_attributes
  assertion_tee_container_image_reference_list = var.assertion_tee_container_image_reference_list
  assertion_tee_container_image_hash_list      = var.assertion_tee_container_image_hash_list

  keystorageservice_alarm_eval_period_sec                = var.keystorageservice_alarm_eval_period_sec
  keystorageservice_alarm_duration_sec                   = var.keystorageservice_alarm_duration_sec
  keystorageservice_cloudfunction_5xx_threshold          = var.keystorageservice_cloudfunction_5xx_threshold
  keystorageservice_cloudfunction_error_threshold        = var.keystorageservice_cloudfunction_error_threshold
  keystorageservice_cloudfunction_max_execution_time_max = var.keystorageservice_cloudfunction_max_execution_time_max
  keystorageservice_lb_5xx_threshold                     = var.keystorageservice_lb_5xx_threshold
  keystorageservice_lb_max_latency_ms                    = var.keystorageservice_lb_max_latency_ms

  encryptionkeyservice_alarm_eval_period_sec                = var.encryptionkeyservice_alarm_eval_period_sec
  encryptionkeyservice_alarm_duration_sec                   = var.encryptionkeyservice_alarm_duration_sec
  encryptionkeyservice_cloudfunction_5xx_threshold          = var.encryptionkeyservice_cloudfunction_5xx_threshold
  encryptionkeyservice_cloudfunction_error_threshold        = var.encryptionkeyservice_cloudfunction_error_threshold
  encryptionkeyservice_cloudfunction_max_execution_time_max = var.encryptionkeyservice_cloudfunction_max_execution_time_max
  encryptionkeyservice_lb_5xx_threshold                     = var.encryptionkeyservice_lb_5xx_threshold
  encryptionkeyservice_lb_max_latency_ms                    = var.encryptionkeyservice_lb_max_latency_ms
}
