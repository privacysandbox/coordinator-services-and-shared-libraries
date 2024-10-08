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
      version = ">= 4.36"
    }
  }
}

provider "google" {
  project = var.project_id
  region  = var.primary_region
}

locals {
  get_public_key_service_zip = var.get_public_key_service_zip != "" ? var.get_public_key_service_zip : "${module.bazel.bazel_bin}/java/com/google/scp/coordinator/keymanagement/keyhosting/service/gcp/PublicKeyServiceHttpCloudFunctionDeploy.zip"
  encryption_key_service_zip = var.encryption_key_service_zip != "" ? var.encryption_key_service_zip : "${module.bazel.bazel_bin}/java/com/google/scp/coordinator/keymanagement/keyhosting/service/gcp/EncryptionKeyServiceHttpCloudFunctionDeploy.zip"
  service_subdomain_suffix   = var.service_subdomain_suffix != null ? var.service_subdomain_suffix : "-${var.environment}"
  public_key_domain          = var.environment != "prod" ? "${var.public_key_service_subdomain}${local.service_subdomain_suffix}.${var.parent_domain_name}" : "${var.public_key_service_subdomain}.${var.parent_domain_name}"
  encryption_key_domain      = var.environment != "prod" ? "${var.encryption_key_service_subdomain}${local.service_subdomain_suffix}.${var.parent_domain_name}" : "${var.encryption_key_service_subdomain}.${var.parent_domain_name}"
  notification_channel_id    = var.alarms_enabled ? google_monitoring_notification_channel.alarm_email[0].id : null
  package_bucket_prefix      = "${var.project_id}_${var.environment}"
  package_bucket_name        = length("${local.package_bucket_prefix}_mpkhs_primary_package_jars") <= 63 ? "${local.package_bucket_prefix}_mpkhs_primary_package_jars" : "${local.package_bucket_prefix}_mpkhs_a_pkg"
}

module "bazel" {
  source = "../../modules/bazel"
}

module "vpc" {
  source = "../../modules/vpc"

  environment = var.environment
  project_id  = var.project_id
  regions     = toset([var.primary_region, var.secondary_region])
}

# Storage bucket containing cloudfunction JARs
resource "google_storage_bucket" "mpkhs_primary_package_bucket" {
  # GCS names are globally unique
  name                        = local.package_bucket_name
  location                    = var.mpkhs_package_bucket_location
  uniform_bucket_level_access = true
  public_access_prevention    = "enforced"
}

resource "google_monitoring_notification_channel" "alarm_email" {
  count        = var.alarms_enabled ? 1 : 0
  display_name = "${var.environment} Coordinator A Key Hosting Alarms Notification Email"
  type         = "email"
  labels = {
    email_address = var.alarms_notification_email
  }
  force_delete = true

  lifecycle {
    # Email should not be empty
    precondition {
      condition     = var.alarms_notification_email != ""
      error_message = "var.alarms_enabled is true with an empty var.alarms_notification_email."
    }
  }
}

module "keydb" {
  source                   = "../../modules/keydb"
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
  package_bucket_name                        = google_storage_bucket.mpkhs_primary_package_bucket.name
  spanner_database_name                      = module.keydb.keydb_name
  spanner_instance_name                      = module.keydb.keydb_instance_name
  cloudfunction_timeout_seconds              = var.cloudfunction_timeout_seconds
  get_public_key_service_zip                 = local.get_public_key_service_zip
  get_public_key_cloudfunction_memory_mb     = var.get_public_key_cloudfunction_memory_mb
  get_public_key_cloudfunction_min_instances = var.get_public_key_cloudfunction_min_instances
  get_public_key_cloudfunction_max_instances = var.get_public_key_cloudfunction_max_instances
  application_name                           = var.application_name

  # Load balance vars
  enable_get_public_key_cdn                    = var.enable_get_public_key_cdn
  get_public_key_cloud_cdn_default_ttl_seconds = var.get_public_key_cloud_cdn_default_ttl_seconds
  get_public_key_cloud_cdn_max_ttl_seconds     = var.get_public_key_cloud_cdn_max_ttl_seconds
  public_key_load_balancer_logs_enabled        = var.public_key_load_balancer_logs_enabled

  # Domain Management
  enable_domain_management                  = var.enable_domain_management
  public_key_domain                         = local.public_key_domain
  public_key_service_alternate_domain_names = var.public_key_service_alternate_domain_names

  # Alarms
  alarms_enabled                                      = var.alarms_enabled
  alarm_eval_period_sec                               = var.get_public_key_alarm_eval_period_sec
  alarm_duration_sec                                  = var.get_public_key_alarm_duration_sec
  notification_channel_id                             = local.notification_channel_id
  get_public_key_cloudfunction_5xx_threshold          = var.get_public_key_cloudfunction_5xx_threshold
  get_public_key_cloudfunction_error_threshold        = var.get_public_key_cloudfunction_error_threshold
  get_public_key_cloudfunction_max_execution_time_max = var.get_public_key_cloudfunction_max_execution_time_max
  get_public_key_lb_5xx_threshold                     = var.get_public_key_lb_5xx_threshold
  get_public_key_lb_max_latency_ms                    = var.get_public_key_lb_max_latency_ms
}

module "encryptionkeyservice" {
  source = "../../modules/encryptionkeyservice"

  project_id                  = var.project_id
  environment                 = var.environment
  region                      = var.primary_region
  allowed_operator_user_group = var.allowed_operator_user_group

  # Function vars
  package_bucket_name                                = google_storage_bucket.mpkhs_primary_package_bucket.name
  spanner_database_name                              = module.keydb.keydb_name
  spanner_instance_name                              = module.keydb.keydb_instance_name
  cloudfunction_timeout_seconds                      = var.cloudfunction_timeout_seconds
  encryption_key_service_zip                         = local.encryption_key_service_zip
  encryption_key_service_cloudfunction_memory_mb     = var.encryption_key_service_cloudfunction_memory_mb
  encryption_key_service_cloudfunction_min_instances = var.encryption_key_service_cloudfunction_min_instances
  encryption_key_service_cloudfunction_max_instances = var.encryption_key_service_cloudfunction_max_instances

  # Domain Management
  enable_domain_management = var.enable_domain_management
  encryption_key_domain    = local.encryption_key_domain

  # Alarms
  alarms_enabled                       = var.alarms_enabled
  alarm_eval_period_sec                = var.encryptionkeyservice_alarm_eval_period_sec
  alarm_duration_sec                   = var.encryptionkeyservice_alarm_duration_sec
  notification_channel_id              = local.notification_channel_id
  cloudfunction_5xx_threshold          = var.encryptionkeyservice_cloudfunction_5xx_threshold
  cloudfunction_error_threshold        = var.encryptionkeyservice_cloudfunction_error_threshold
  cloudfunction_max_execution_time_max = var.encryptionkeyservice_cloudfunction_max_execution_time_max
  lb_5xx_threshold                     = var.encryptionkeyservice_lb_5xx_threshold
  lb_max_latency_ms                    = var.encryptionkeyservice_lb_max_latency_ms
}

module "domain_a_records" {
  source = "../../modules/domain_a_records"

  enable_domain_management = var.enable_domain_management

  parent_domain_name         = var.parent_domain_name
  parent_domain_name_project = var.parent_domain_name_project

  service_domain_to_address_map = var.enable_domain_management ? {
    (local.public_key_domain) : module.publickeyhostingservice.get_public_key_loadbalancer_ip,
    (local.encryption_key_domain) : module.encryptionkeyservice.encryption_key_service_loadbalancer_ip
  } : {}
}

# parameters

module "keydb_instance_id" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "SPANNER_INSTANCE"
  parameter_value = module.keydb.keydb_instance_name
}

module "keydb_name" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "KEY_DB_NAME"
  parameter_value = module.keydb.keydb_name
}

module "kms_key_uri" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "KMS_KEY_URI"
  parameter_value = "gcp-kms://${module.keygenerationservice.key_encryption_key_id}"
}

module "pubsub_id" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "SUBSCRIPTION_ID"
  parameter_value = module.keygenerationservice.subscription_id
}

module "key_generation_count" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "NUMBER_OF_KEYS_TO_CREATE"
  parameter_value = var.key_generation_count
}

module "key_generation_validity_in_days" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "KEYS_VALIDITY_IN_DAYS"
  parameter_value = var.key_generation_validity_in_days
}

module "key_generation_ttl_in_days" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "KEY_TTL_IN_DAYS"
  parameter_value = var.key_generation_ttl_in_days
}

module "peer_coordinator_kms_key_uri" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "PEER_COORDINATOR_KMS_KEY_URI"
  parameter_value = "gcp-kms://${var.peer_coordinator_kms_key_uri}"
}

module "key_storage_service_base_url" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "KEY_STORAGE_SERVICE_BASE_URL"
  parameter_value = var.key_storage_service_base_url
}

module "key_storage_service_cloudfunction_url" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "KEY_STORAGE_SERVICE_CLOUDFUNCTION_URL"
  parameter_value = var.key_storage_service_cloudfunction_url
}

module "peer_coordinator_wip_provider" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "PEER_COORDINATOR_WIP_PROVIDER"
  parameter_value = var.peer_coordinator_wip_provider
}

module "peer_coordinator_service_account" {
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "PEER_COORDINATOR_SERVICE_ACCOUNT"
  parameter_value = var.peer_coordinator_service_account
}

module "key_id_type" {
  count           = var.key_id_type == "" ? 0 : 1
  source          = "../../modules/parameters"
  environment     = var.environment
  parameter_name  = "KEY_ID_TYPE"
  parameter_value = var.key_id_type
}
