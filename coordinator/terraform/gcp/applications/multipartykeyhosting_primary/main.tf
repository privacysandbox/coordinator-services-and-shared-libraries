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

terraform {
  required_providers {
    google = {
      source  = "hashicorp/google"
      version = ">= 6.29.0"
    }
  }
}

locals {
  service_subdomain_suffix = var.service_subdomain_suffix != null ? var.service_subdomain_suffix : "-${var.environment}"
  parent_domain_name       = var.parent_domain_name != null ? var.parent_domain_name : ""
  public_key_domain        = var.environment != "prod" ? "${var.public_key_service_subdomain}${local.service_subdomain_suffix}.${local.parent_domain_name}" : "${var.public_key_service_subdomain}.${local.parent_domain_name}"
  encryption_key_domain    = var.environment != "prod" ? "${var.encryption_key_service_subdomain}${local.service_subdomain_suffix}.${local.parent_domain_name}" : "${var.encryption_key_service_subdomain}.${local.parent_domain_name}"
  notification_channel_id  = var.alarms_enabled ? google_monitoring_notification_channel.alarm_email[0].id : null
}

module "vpc" {
  source = "../../modules/vpc"

  environment = var.environment
  project_id  = var.project_id
  regions     = toset([var.primary_region, var.secondary_region])
}

resource "google_monitoring_notification_channel" "alarm_email" {
  count = var.alarms_enabled ? 1 : 0

  project      = var.project_id
  display_name = "${var.environment} Coordinator A Key Hosting Alarms Notification Email"
  type         = "email"
  labels = {
    email_address = var.alarms_notification_email
  }
  force_delete = true

  lifecycle {
    # Email should not be empty
    precondition {
      condition     = var.alarms_notification_email != null
      error_message = "var.alarms_enabled is true with an empty var.alarms_notification_email."
    }
  }
}

module "keydb" {
  source = "../../modules/keydb"

  project_id               = var.project_id
  environment              = var.environment
  spanner_instance_config  = var.spanner_instance_config
  spanner_processing_units = var.spanner_processing_units
  key_db_retention_period  = var.key_db_retention_period
}

module "keygenerationservice" {
  source = "../../modules/keygenerationservice"

  project_id                     = var.project_id
  environment                    = var.environment
  network                        = module.vpc.network
  region                         = var.primary_region
  egress_internet_tag            = module.vpc.egress_internet_tag
  key_gen_instance_force_replace = var.key_gen_instance_force_replace

  # Data args
  key_generation_image              = var.key_generation_image
  spanner_database_name             = module.keydb.keydb_name
  spanner_instance_name             = module.keydb.keydb_instance_name
  key_generation_logging_enabled    = var.key_generation_logging_enabled
  key_generation_monitoring_enabled = var.key_generation_monitoring_enabled

  # Business Rules Args
  enable_key_generation         = var.enable_key_generation
  key_generation_cron_schedule  = var.key_generation_cron_schedule
  key_generation_cron_time_zone = var.key_generation_cron_time_zone
  key_generation_tee_allowed_sa = var.key_generation_tee_allowed_sa

  # TEE Args
  instance_disk_image               = var.instance_disk_image
  key_generation_tee_restart_policy = var.key_generation_tee_restart_policy

  # Monitoring Args
  alarms_enabled                  = var.alarms_enabled
  keydb_instance_name             = module.keydb.keydb_instance_name
  notification_channel_id         = local.notification_channel_id
  key_generation_alignment_period = var.key_generation_alignment_period
  undelivered_messages_threshold  = var.key_generation_undelivered_messages_threshold
}

module "publickeyhostingservice" {
  source = "../../modules/publickeyhostingservice"

  project_id  = var.project_id
  environment = var.environment
  regions     = [var.primary_region, var.secondary_region]

  # Function vars
  spanner_database_name                      = module.keydb.keydb_name
  spanner_instance_name                      = module.keydb.keydb_instance_name
  cloudfunction_timeout_seconds              = var.cloudfunction_timeout_seconds
  get_public_key_cloudfunction_memory_mb     = var.get_public_key_cloudfunction_memory_mb
  get_public_key_cloudfunction_min_instances = var.get_public_key_cloudfunction_min_instances
  get_public_key_cloudfunction_max_instances = var.get_public_key_cloudfunction_max_instances
  get_public_key_request_concurrency         = var.get_public_key_request_concurrency
  get_public_key_cpus                        = var.get_public_key_cpus
  application_name                           = var.application_name

  # Cloud Run vars
  cloud_run_revision_force_replace  = var.cloud_run_revision_force_replace
  public_key_service_image          = var.public_key_service_image
  public_key_service_canary_percent = var.public_key_service_canary_percent

  # Load balance vars
  enable_get_public_key_cdn                    = var.enable_get_public_key_cdn
  get_public_key_cloud_cdn_default_ttl_seconds = var.get_public_key_cloud_cdn_default_ttl_seconds
  get_public_key_cloud_cdn_max_ttl_seconds     = var.get_public_key_cloud_cdn_max_ttl_seconds
  public_key_load_balancer_logs_enabled        = var.public_key_load_balancer_logs_enabled
  parent_domain_name                           = var.parent_domain_name

  # Domain Management
  enable_domain_management                  = var.enable_domain_management
  public_key_domain                         = local.public_key_domain
  public_key_service_alternate_domain_names = var.public_key_service_alternate_domain_names
  enable_public_key_alternative_domain      = var.enable_public_key_alternative_domain
  disable_public_key_ssl_cert               = var.disable_public_key_ssl_cert
  remove_public_key_ssl_cert                = var.remove_public_key_ssl_cert
  enable_public_key_certificate_map         = var.enable_public_key_certificate_map

  # OTel Metrics
  export_otel_metrics = var.export_otel_metrics

  # Cloud Armor vars
  enable_security_policy              = var.enable_security_policy
  use_adaptive_protection             = var.use_adaptive_protection
  public_key_ddos_thresholds          = var.public_key_ddos_thresholds
  public_key_security_policy_rules    = var.public_key_security_policy_rules
  enable_cloud_armor_alerts           = var.enable_cloud_armor_alerts
  enable_cloud_armor_notifications    = var.enable_cloud_armor_notifications
  cloud_armor_notification_channel_id = var.cloud_armor_notification_channel_id
}

module "encryptionkeyservice" {
  source = "../../modules/encryptionkeyservice"

  project_id                  = var.project_id
  environment                 = var.environment
  region                      = var.primary_region
  allowed_operator_user_group = var.allowed_operator_user_group

  # Function vars
  spanner_database_name                              = module.keydb.keydb_name
  spanner_instance_name                              = module.keydb.keydb_instance_name
  cloudfunction_timeout_seconds                      = var.cloudfunction_timeout_seconds
  encryption_key_service_cloudfunction_memory_mb     = var.encryption_key_service_cloudfunction_memory_mb
  encryption_key_service_cloudfunction_min_instances = var.encryption_key_service_cloudfunction_min_instances
  encryption_key_service_cloudfunction_max_instances = var.encryption_key_service_cloudfunction_max_instances
  encryption_key_service_request_concurrency         = var.encryption_key_service_request_concurrency
  encryption_key_service_cpus                        = var.encryption_key_service_cpus

  # Cloud Run vars
  cloud_run_revision_force_replace     = var.cloud_run_revision_force_replace
  private_key_service_image            = var.private_key_service_image
  private_key_service_custom_audiences = var.private_key_service_custom_audiences
  private_key_service_canary_percent   = var.private_key_service_canary_percent

  # Domain Management
  enable_domain_management = var.enable_domain_management
  encryption_key_domain    = local.encryption_key_domain

  # OTel Metrics
  export_otel_metrics = var.export_otel_metrics

  # Cloud Armor vars
  enable_security_policy               = var.enable_security_policy
  use_adaptive_protection              = var.use_adaptive_protection
  encryption_key_ddos_thresholds       = var.encryption_key_ddos_thresholds
  encryption_key_security_policy_rules = var.encryption_key_security_policy_rules
  enable_cloud_armor_alerts            = var.enable_cloud_armor_alerts
  enable_cloud_armor_notifications     = var.enable_cloud_armor_notifications
  cloud_armor_notification_channel_id  = var.cloud_armor_notification_channel_id
}

module "domain_a_records" {
  source = "../../modules/domain_a_records"

  enable_domain_management = var.enable_domain_management

  parent_domain_name         = var.parent_domain_name
  parent_domain_name_project = var.parent_domain_name_project

  service_domain_to_address_map = var.enable_domain_management ? {
    (local.public_key_domain) : module.publickeyhostingservice.public_key_service_cloud_run_loadbalancer_ip,
    (local.encryption_key_domain) : module.encryptionkeyservice.encryption_key_service_cloud_run_loadbalancer_ip,
  } : {}
}

# parameters

module "keydb_instance_id" {
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "SPANNER_INSTANCE"
  parameter_value = module.keydb.keydb_instance_name
}

module "keydb_name" {
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "KEY_DB_NAME"
  parameter_value = module.keydb.keydb_name
}

module "kms_key_uri" {
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "KMS_KEY_URI"
  parameter_value = "gcp-kms://${module.keygenerationservice.key_encryption_key_id}"
}

module "pubsub_id" {
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "SUBSCRIPTION_ID"
  parameter_value = module.keygenerationservice.subscription_id
}

module "key_generation_count" {
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "NUMBER_OF_KEYS_TO_CREATE"
  parameter_value = var.key_generation_count
}

module "key_generation_validity_in_days" {
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "KEYS_VALIDITY_IN_DAYS"
  parameter_value = var.key_generation_validity_in_days
}

module "key_generation_ttl_in_days" {
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "KEY_TTL_IN_DAYS"
  parameter_value = var.key_generation_ttl_in_days
}

module "peer_coordinator_kms_key_uri" {
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "PEER_COORDINATOR_KMS_KEY_URI"
  parameter_value = "gcp-kms://${var.peer_coordinator_kms_key_uri}"
}

module "key_storage_service_base_url" {
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "KEY_STORAGE_SERVICE_BASE_URL"
  parameter_value = var.key_storage_service_base_url
}

module "key_storage_service_cloudfunction_url" {
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "KEY_STORAGE_SERVICE_CLOUDFUNCTION_URL"
  parameter_value = var.key_storage_service_cloudfunction_url
}

module "peer_coordinator_wip_provider" {
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "PEER_COORDINATOR_WIP_PROVIDER"
  parameter_value = var.peer_coordinator_wip_provider
}

module "peer_coordinator_service_account" {
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "PEER_COORDINATOR_SERVICE_ACCOUNT"
  parameter_value = var.peer_coordinator_service_account
}

module "key_id_type" {
  count  = var.key_id_type == null ? 0 : 1
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "KEY_ID_TYPE"
  parameter_value = var.key_id_type
}

module "aws_xc_enabled" {
  count  = var.aws_xc_enabled ? 1 : 0
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "AWS_XC_ENABLED"
  parameter_value = "true"
}

module "aws_kms_key_uri" {
  count  = var.aws_xc_enabled ? 1 : 0
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "AWS_KMS_KEY_URI"
  parameter_value = "aws-kms://${var.aws_kms_key_encryption_key_arn}"
}

module "aws_kms_role_arn" {
  count  = var.aws_xc_enabled ? 1 : 0
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "AWS_KMS_ROLE_ARN"
  parameter_value = var.aws_kms_key_encryption_key_role_arn
}

module "aws_key_sync_enabled" {
  count  = var.aws_key_sync_enabled ? 1 : 0
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "AWS_KEY_SYNC_ENABLED"
  parameter_value = "true"
}

module "aws_key_sync_role_arn" {
  count  = var.aws_key_sync_enabled ? 1 : 0
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "AWS_KEY_SYNC_ROLE_ARN"
  parameter_value = var.aws_key_sync_role_arn
}

module "aws_key_sync_kms_key_uri" {
  count  = var.aws_key_sync_enabled ? 1 : 0
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "AWS_KEY_SYNC_KMS_KEY_URI"
  parameter_value = "aws-kms://${var.aws_key_sync_kms_key_uri}"
}

module "aws_key_sync_keydb_region" {
  count  = var.aws_key_sync_enabled ? 1 : 0
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "AWS_KEY_SYNC_KEYDB_REGION"
  parameter_value = var.aws_key_sync_keydb_region
}

module "aws_key_sync_keydb_table_name" {
  count  = var.aws_key_sync_enabled ? 1 : 0
  source = "../../modules/parameters"

  project_id      = var.project_id
  environment     = var.environment
  parameter_name  = "AWS_KEY_SYNC_KEYDB_TABLE_NAME"
  parameter_value = var.aws_key_sync_keydb_table_name
}
