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

  enable_domain_management         = var.enable_domain_management
  parent_domain_name               = var.parent_domain_name
  parent_domain_name_project       = var.parent_domain_name_project
  service_subdomain_suffix         = var.service_subdomain_suffix
  encryption_key_service_subdomain = var.encryption_key_service_subdomain
  key_storage_service_subdomain    = var.key_storage_service_subdomain

  aws_xc_enabled                      = var.aws_xc_enabled
  aws_kms_key_encryption_key_arn      = var.aws_kms_key_encryption_key_arn
  aws_kms_key_encryption_key_role_arn = var.aws_kms_key_encryption_key_role_arn

  cloudfunction_timeout_seconds                      = var.cloudfunction_timeout_seconds
  encryption_key_service_cloudfunction_memory_mb     = var.encryption_key_service_cloudfunction_memory_mb
  encryption_key_service_cloudfunction_min_instances = var.encryption_key_service_cloudfunction_min_instances
  encryption_key_service_cloudfunction_max_instances = var.encryption_key_service_cloudfunction_max_instances
  encryption_key_service_request_concurrency         = var.encryption_key_service_request_concurrency
  encryption_key_service_cpus                        = var.encryption_key_service_cpus
  key_storage_service_cloudfunction_memory_mb        = var.key_storage_service_cloudfunction_memory_mb
  key_storage_service_cloudfunction_min_instances    = var.key_storage_service_cloudfunction_min_instances
  key_storage_service_cloudfunction_max_instances    = var.key_storage_service_cloudfunction_max_instances

  spanner_instance_config  = var.spanner_instance_config
  spanner_processing_units = var.spanner_processing_units
  key_db_retention_period  = var.key_db_retention_period

  allowed_wip_iam_principals                   = var.allowed_wip_iam_principals
  allowed_wip_user_group                       = var.allowed_wip_user_group
  enable_attestation                           = var.enable_attestation
  assertion_tee_swname                         = var.assertion_tee_swname
  assertion_tee_support_attributes             = var.assertion_tee_support_attributes
  assertion_tee_container_image_reference_list = var.assertion_tee_container_image_reference_list
  assertion_tee_container_image_hash_list      = var.assertion_tee_container_image_hash_list

  export_otel_metrics = var.export_otel_metrics

  cloud_run_revision_force_replace     = var.cloud_run_revision_force_replace
  private_key_service_image            = var.private_key_service_image
  private_key_service_custom_audiences = var.private_key_service_custom_audiences
  key_storage_service_image            = var.key_storage_service_image
  key_storage_service_custom_audiences = var.key_storage_service_custom_audiences

  aws_key_sync_enabled          = var.aws_key_sync_enabled
  aws_key_sync_role_arn         = var.aws_key_sync_role_arn
  aws_key_sync_kms_key_uri      = var.aws_key_sync_kms_key_uri
  aws_key_sync_keydb_region     = var.aws_key_sync_keydb_region
  aws_key_sync_keydb_table_name = var.aws_key_sync_keydb_table_name

  enable_security_policy               = var.enable_security_policy
  use_adaptive_protection              = var.use_adaptive_protection
  encryption_key_security_policy_rules = var.encryption_key_security_policy_rules
  key_storage_security_policy_rules    = var.key_storage_security_policy_rules
  enable_cloud_armor_alerts            = var.enable_cloud_armor_alerts
  enable_cloud_armor_notifications     = var.enable_cloud_armor_notifications
  cloud_armor_notification_channel_id  = var.cloud_armor_notification_channel_id
}
