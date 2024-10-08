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
  service_subdomain_suffix   = var.service_subdomain_suffix != null ? var.service_subdomain_suffix : "-${var.environment}"
  encryption_key_service_zip = var.encryption_key_service_zip != "" ? var.encryption_key_service_zip : "${module.bazel.bazel_bin}/java/com/google/scp/coordinator/keymanagement/keyhosting/service/gcp/EncryptionKeyServiceHttpCloudFunctionDeploy.zip"
  key_storage_service_zip    = var.key_storage_service_zip != "" ? var.key_storage_service_zip : "${module.bazel.bazel_bin}/java/com/google/scp/coordinator/keymanagement/keystorage/service/gcp/KeyStorageServiceHttpCloudFunctionDeploy.zip"
  key_storage_domain         = var.environment != "prod" ? "${var.key_storage_service_subdomain}${local.service_subdomain_suffix}.${var.parent_domain_name}" : "${var.key_storage_service_subdomain}.${var.parent_domain_name}"
  encryption_key_domain      = var.environment != "prod" ? "${var.encryption_key_service_subdomain}${local.service_subdomain_suffix}.${var.parent_domain_name}" : "${var.encryption_key_service_subdomain}.${var.parent_domain_name}"
  notification_channel_id    = var.alarms_enabled ? google_monitoring_notification_channel.alarm_email[0].id : null
  package_bucket_prefix      = "${var.project_id}_${var.environment}"
  package_bucket_name        = length("${local.package_bucket_prefix}_mpkhs_secondary_package_jars") <= 63 ? "${local.package_bucket_prefix}_mpkhs_secondary_package_jars" : "${local.package_bucket_prefix}_mpkhs_b_pkg"
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
resource "google_storage_bucket" "mpkhs_secondary_package_bucket" {
  # GCS names are globally unique
  name                        = local.package_bucket_name
  location                    = var.mpkhs_package_bucket_location
  uniform_bucket_level_access = true
  public_access_prevention    = "enforced"
}

# Cloud KMS encryption ring and key encryption key (KEK)
resource "google_kms_key_ring" "key_encryption_ring" {
  name     = "${var.environment}_key_encryption_ring"
  location = "us"
}

resource "google_kms_crypto_key" "key_encryption_key" {
  name     = "${var.environment}_key_encryption_key"
  key_ring = google_kms_key_ring.key_encryption_ring.id

  lifecycle {
    prevent_destroy = true
  }
}

resource "google_monitoring_notification_channel" "alarm_email" {
  count        = var.alarms_enabled ? 1 : 0
  display_name = "${var.environment} Coordinator B Key Hosting Alarms Notification Email"
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

module "keystorageservice" {
  source = "../../modules/keystorageservice"

  project_id                 = var.project_id
  environment                = var.environment
  region                     = var.primary_region
  allowed_wip_iam_principals = var.allowed_wip_iam_principals
  allowed_wip_user_group     = var.allowed_wip_user_group

  # Function vars
  key_encryption_key_id                           = google_kms_crypto_key.key_encryption_key.id
  package_bucket_name                             = google_storage_bucket.mpkhs_secondary_package_bucket.name
  load_balancer_name                              = "keystoragelb"
  spanner_database_name                           = module.keydb.keydb_name
  spanner_instance_name                           = module.keydb.keydb_instance_name
  cloudfunction_timeout_seconds                   = var.cloudfunction_timeout_seconds
  key_storage_cloudfunction_name                  = "key-storage-cloudfunction"
  key_storage_service_zip                         = local.key_storage_service_zip
  key_storage_cloudfunction_memory                = var.key_storage_service_cloudfunction_memory_mb
  key_storage_service_cloudfunction_min_instances = var.key_storage_service_cloudfunction_min_instances
  key_storage_service_cloudfunction_max_instances = var.key_storage_service_cloudfunction_max_instances

  # Domain Management
  enable_domain_management = var.enable_domain_management
  key_storage_domain       = local.key_storage_domain

  # Alarms
  alarms_enabled                       = var.alarms_enabled
  alarm_eval_period_sec                = var.keystorageservice_alarm_eval_period_sec
  alarm_duration_sec                   = var.keystorageservice_alarm_duration_sec
  notification_channel_id              = local.notification_channel_id
  cloudfunction_5xx_threshold          = var.keystorageservice_cloudfunction_5xx_threshold
  cloudfunction_error_threshold        = var.keystorageservice_cloudfunction_error_threshold
  cloudfunction_max_execution_time_max = var.keystorageservice_cloudfunction_max_execution_time_max
  lb_5xx_threshold                     = var.keystorageservice_lb_5xx_threshold
  lb_max_latency_ms                    = var.keystorageservice_lb_max_latency_ms
}

module "encryptionkeyservice" {
  source = "../../modules/encryptionkeyservice"

  project_id                  = var.project_id
  environment                 = var.environment
  region                      = var.primary_region
  allowed_operator_user_group = var.allowed_operator_user_group

  # Function vars
  package_bucket_name                                = google_storage_bucket.mpkhs_secondary_package_bucket.name
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
    (local.key_storage_domain) : module.keystorageservice.load_balancer_ip,
    (local.encryption_key_domain) : module.encryptionkeyservice.encryption_key_service_loadbalancer_ip
  } : {}
}

# Coordinator WIP Provider
module "workload_identity_pool" {
  source                                          = "../../modules/workloadidentitypoolprovider"
  project_id                                      = var.wipp_project_id_override == null ? var.project_id : var.wipp_project_id_override
  wip_allowed_service_account_project_id_override = var.wip_allowed_service_account_project_id_override == null ? var.project_id : var.wip_allowed_service_account_project_id_override
  environment                                     = var.environment

  # Limited to 32 characters
  workload_identity_pool_id          = "${var.environment}-cowip"
  workload_identity_pool_description = "Workload Identity Pool to manage Peer KEK access using attestation."

  # Limited to 32 characters
  workload_identity_pool_provider_id = "${var.environment}-cowip-pvdr"
  wip_provider_description           = "WIP Provider to manage OIDC tokens and attestation."

  # Limited to 30 characters
  wip_verified_service_account_id           = "${var.environment}-coverifiedusr"
  wip_verified_service_account_display_name = "${var.environment} Verified Coordinator User"

  # Limited to 30 characters
  wip_allowed_service_account_id           = "${var.environment}-coallowedusr"
  wip_allowed_service_account_display_name = "${var.environment} Allowed Coordinator User"

  key_encryption_key_id                        = google_kms_crypto_key.key_encryption_key.id
  allowed_wip_iam_principals                   = var.allowed_wip_iam_principals
  allowed_wip_user_group                       = var.allowed_wip_user_group
  enable_attestation                           = var.enable_attestation
  assertion_tee_swname                         = var.assertion_tee_swname
  assertion_tee_support_attributes             = var.assertion_tee_support_attributes
  assertion_tee_container_image_reference_list = var.assertion_tee_container_image_reference_list
  assertion_tee_container_image_hash_list      = var.assertion_tee_container_image_hash_list

  enable_audit_log = var.enable_audit_log
}

# Allow Verified Service Account to encrypt with given KEK
resource "google_kms_crypto_key_iam_member" "workload_identity_member" {
  crypto_key_id = google_kms_crypto_key.key_encryption_key.id
  role          = "roles/cloudkms.cryptoKeyEncrypter"
  member        = "serviceAccount:${module.workload_identity_pool.wip_verified_service_account}"
}
