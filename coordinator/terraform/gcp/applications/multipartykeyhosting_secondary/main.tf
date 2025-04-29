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
  service_subdomain_suffix        = var.service_subdomain_suffix != null ? var.service_subdomain_suffix : "-${var.environment}"
  parent_domain_name              = var.parent_domain_name != null ? var.parent_domain_name : ""
  key_storage_domain              = var.environment != "prod" ? "${var.key_storage_service_subdomain}${local.service_subdomain_suffix}.${local.parent_domain_name}" : "${var.key_storage_service_subdomain}.${local.parent_domain_name}"
  encryption_key_domain           = var.environment != "prod" ? "${var.encryption_key_service_subdomain}${local.service_subdomain_suffix}.${local.parent_domain_name}" : "${var.encryption_key_service_subdomain}.${local.parent_domain_name}"
  encryption_key_cloud_run_domain = var.environment != "prod" ? "${var.encryption_key_service_subdomain}-cr${local.service_subdomain_suffix}.${local.parent_domain_name}" : "${var.encryption_key_service_subdomain}-cr.${local.parent_domain_name}"
}

module "vpc" {
  source = "../../modules/vpc"

  environment = var.environment
  project_id  = var.project_id
  regions     = toset([var.primary_region, var.secondary_region])
}

# Cloud KMS encryption ring and key encryption key (KEK)
resource "google_kms_key_ring" "key_encryption_ring" {
  project  = var.project_id
  name     = "${var.environment}_key_encryption_ring"
  location = "us"
}

resource "google_kms_crypto_key" "key_encryption_key" {
  name     = "${var.environment}_key_encryption_key"
  key_ring = google_kms_key_ring.key_encryption_ring.id

  # Setting KMS key rotation to yearly
  rotation_period = "31536000s"

  lifecycle {
    prevent_destroy = true
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

module "keystorageservice" {
  source = "../../modules/keystorageservice"

  project_id                 = var.project_id
  environment                = var.environment
  region                     = var.primary_region
  allowed_wip_iam_principals = var.allowed_wip_iam_principals
  allowed_wip_user_group     = var.allowed_wip_user_group

  # Function vars
  key_encryption_key_id                           = google_kms_crypto_key.key_encryption_key.id
  load_balancer_name                              = "keystoragelb"
  spanner_database_name                           = module.keydb.keydb_name
  spanner_instance_name                           = module.keydb.keydb_instance_name
  cloudfunction_timeout_seconds                   = var.cloudfunction_timeout_seconds
  key_storage_cloudfunction_memory                = var.key_storage_service_cloudfunction_memory_mb
  key_storage_service_cloudfunction_min_instances = var.key_storage_service_cloudfunction_min_instances
  key_storage_service_cloudfunction_max_instances = var.key_storage_service_cloudfunction_max_instances

  # AWS cross-cloud variables
  aws_xc_enabled                      = var.aws_xc_enabled
  aws_kms_key_encryption_key_arn      = var.aws_kms_key_encryption_key_arn
  aws_kms_key_encryption_key_role_arn = var.aws_kms_key_encryption_key_role_arn

  # Cloud Run vars
  cloud_run_revision_force_replace     = var.cloud_run_revision_force_replace
  key_storage_service_image            = var.key_storage_service_image
  key_storage_service_custom_audiences = var.key_storage_service_custom_audiences

  # Domain Management
  enable_domain_management = var.enable_domain_management
  key_storage_domain       = local.key_storage_domain

  # OTel Metrics
  export_otel_metrics = var.export_otel_metrics

  # AWS cross-cloud key sync variables
  aws_key_sync_enabled          = var.aws_key_sync_enabled
  aws_key_sync_role_arn         = var.aws_key_sync_role_arn
  aws_key_sync_kms_key_uri      = var.aws_key_sync_kms_key_uri
  aws_key_sync_keydb_region     = var.aws_key_sync_keydb_region
  aws_key_sync_keydb_table_name = var.aws_key_sync_keydb_table_name

  # Cloud Armor vars
  enable_security_policy            = var.enable_security_policy
  use_adaptive_protection           = var.use_adaptive_protection
  key_storage_security_policy_rules = var.key_storage_security_policy_rules
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

  # Domain Management
  enable_domain_management = var.enable_domain_management
  encryption_key_domain    = local.encryption_key_domain

  # OTel Metrics
  export_otel_metrics = var.export_otel_metrics

  # Cloud Armor vars
  enable_security_policy               = var.enable_security_policy
  use_adaptive_protection              = var.use_adaptive_protection
  encryption_key_security_policy_rules = var.encryption_key_security_policy_rules
}

module "domain_a_records" {
  source = "../../modules/domain_a_records"

  enable_domain_management = var.enable_domain_management

  parent_domain_name         = var.parent_domain_name
  parent_domain_name_project = var.parent_domain_name_project

  service_domain_to_address_map = var.enable_domain_management ? {
    (local.key_storage_domain) : module.keystorageservice.load_balancer_ip_cloud_run,
    (local.encryption_key_domain) : module.encryptionkeyservice.encryption_key_service_cloud_run_loadbalancer_ip,
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
