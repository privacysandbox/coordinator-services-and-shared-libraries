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
  pbs_auth_allowed_principals           = var.pbs_remote_coordinator_service_account_email != null ? concat(var.pbs_auth_allowed_principals, ["serviceAccount:${var.pbs_service_account_email}", "serviceAccount:${var.pbs_remote_coordinator_service_account_email}"]) : concat(var.pbs_auth_allowed_principals, ["serviceAccount:${var.pbs_service_account_email}"])
  pbs_artifact_registry_repository_name = var.pbs_artifact_registry_repository_name != null ? var.pbs_artifact_registry_repository_name : "${var.environment}-scp-pbs-artifact-registry-repo"
  service_subdomain_suffix              = var.service_subdomain_suffix != null ? var.service_subdomain_suffix : "-${var.environment}"
  pbs_domain                            = var.enable_domain_management ? "${var.service_subdomain}${local.service_subdomain_suffix}.${var.parent_domain_name}" : ""
  pbs_service_endpoint                  = var.enable_domain_management ? "https://${local.pbs_domain}" : "https://${google_compute_global_address.pbs_ip_address.address}"
}

# Reserve IP address for PBS.
resource "google_compute_global_address" "pbs_ip_address" {
  project = var.project
  name    = "${var.environment}-pbs-ip-address"
}

module "pbs_a_record" {
  count  = var.enable_domain_management ? 1 : 0
  source = "../../modules/distributedpbs_domain_a_record"

  parent_domain_project = var.parent_domain_project
  parent_domain_name    = var.parent_domain_name
  pbs_ip_address        = google_compute_global_address.pbs_ip_address.address
  pbs_domain            = local.pbs_domain
}

module "pbs_auth_storage" {
  source = "../../modules/distributedpbs_auth_db"

  project_id                                    = var.project
  environment                                   = var.environment
  region                                        = var.region
  pbs_auth_spanner_instance_config              = var.pbs_auth_spanner_instance_config
  pbs_auth_spanner_instance_edition             = var.pbs_auth_spanner_instance_edition
  pbs_auth_spanner_instance_processing_units    = var.pbs_auth_spanner_instance_processing_units
  pbs_auth_spanner_database_name_suffix         = var.pbs_auth_spanner_database_name_suffix
  pbs_auth_spanner_database_retention_period    = var.pbs_auth_spanner_database_retention_period
  pbs_auth_spanner_database_deletion_protection = var.pbs_auth_spanner_database_deletion_protection
}

module "pbs_auth" {
  source = "../../modules/distributedpbs_auth_service"

  project_id  = var.project
  environment = var.environment
  region      = var.region

  auth_cloud_function_handler_path              = var.auth_cloud_function_handler_path
  pbs_auth_cloudfunction_min_instances          = var.pbs_auth_cloudfunction_min_instances
  pbs_auth_cloudfunction_max_instances          = var.pbs_auth_cloudfunction_max_instances
  pbs_auth_cloudfunction_timeout_seconds        = var.pbs_auth_cloudfunction_timeout_seconds
  pbs_auth_cloudfunction_instance_concurrency   = var.pbs_auth_cloudfunction_instance_concurrency
  pbs_auth_cloudfunction_instance_available_cpu = var.pbs_auth_cloudfunction_instance_available_cpu
  pbs_auth_cloudfunction_memory_mb              = var.pbs_auth_cloudfunction_memory_mb
  pbs_auth_package_bucket_force_destroy         = var.pbs_auth_package_bucket_force_destroy
  pbs_auth_spanner_instance_name                = module.pbs_auth_storage.pbs_auth_spanner_instance_name
  pbs_auth_spanner_database_name                = module.pbs_auth_storage.pbs_auth_spanner_database_name
  pbs_auth_v2_spanner_table_name                = module.pbs_auth_storage.pbs_auth_spanner_authorization_v2_table_name
  pbs_auth_allowed_principals                   = local.pbs_auth_allowed_principals
  pbs_auth_package_bucket                       = var.pbs_auth_package_bucket
  pbs_auth_package_path                         = var.pbs_auth_package_path
  limit_traffic_to_internal_and_lb              = var.enable_domain_management

  depends_on = [
    module.pbs_auth_storage
  ]
}

module "pbs_storage" {
  source = "../../modules/distributedpbs_storage"

  project_id                                     = var.project
  environment                                    = var.environment
  region                                         = var.region
  pbs_cloud_storage_journal_bucket_force_destroy = var.pbs_cloud_storage_journal_bucket_force_destroy
  pbs_cloud_storage_journal_bucket_versioning    = var.pbs_cloud_storage_journal_bucket_versioning
  pbs_spanner_instance_config                    = var.pbs_spanner_instance_config
  pbs_spanner_instance_edition                   = var.pbs_spanner_instance_edition
  pbs_spanner_instance_processing_units          = var.pbs_spanner_instance_processing_units
  pbs_spanner_database_name_suffix               = var.pbs_spanner_database_name_suffix
  pbs_spanner_database_retention_period          = var.pbs_spanner_database_retention_period
  pbs_spanner_database_deletion_protection       = var.pbs_spanner_database_deletion_protection
  pbs_spanner_autoscaling_config                 = var.pbs_spanner_autoscaling_config
}

module "pbs_managed_instance_group_environment" {
  source = "../../modules/distributedpbs_managed_instance_group_environment"

  project_id  = var.project
  environment = var.environment
  region      = var.region

  pbs_artifact_registry_repository_name = local.pbs_artifact_registry_repository_name
  pbs_image_override                    = var.pbs_image_override
  pbs_image_tag                         = var.pbs_image_tag

  pbs_cloud_storage_journal_bucket_name = module.pbs_storage.pbs_cloud_storage_journal_bucket_name
  pbs_spanner_instance_name             = module.pbs_storage.pbs_spanner_instance_name
  pbs_spanner_database_name             = module.pbs_storage.pbs_spanner_database_name
  pbs_spanner_budget_key_table_name     = module.pbs_storage.pbs_spanner_budget_key_table_name
  pbs_spanner_partition_lock_table_name = module.pbs_storage.pbs_spanner_partition_lock_table_name

  pbs_application_environment_variables = var.pbs_application_environment_variables
  pbs_auth_audience_url                 = module.pbs_auth.pbs_auth_audience_url
  pbs_service_account_email             = var.pbs_service_account_email
  enable_domain_management              = var.enable_domain_management
  pbs_domain                            = "https://${local.pbs_domain}"

  main_port = var.pbs_main_port

  pbs_cloud_run_min_instances          = var.pbs_cloud_run_min_instances
  pbs_cloud_run_max_instances          = var.pbs_cloud_run_max_instances
  pbs_cloud_run_max_concurrency        = var.pbs_cloud_run_max_concurrency
  pbs_cloud_run_revision_force_replace = var.pbs_cloud_run_revision_force_replace
}

module "pbs_lb" {
  source = "../../modules/distributedpbs_load_balancer"

  environment = var.environment
  region      = var.region
  project_id  = var.project

  enable_domain_management       = var.enable_domain_management
  pbs_domain                     = local.pbs_domain
  parent_domain_name             = var.parent_domain_name
  parent_domain_project          = var.parent_domain_project
  pbs_ip_address                 = google_compute_global_address.pbs_ip_address.address
  pbs_auth_cloudfunction_name    = module.pbs_auth.pbs_auth_cloudfunction_name
  pbs_cloud_run_name             = module.pbs_managed_instance_group_environment.pbs_cloud_run_name
  pbs_tls_alternate_names        = var.pbs_tls_alternate_names
  certificate_manager_has_prefix = var.certificate_manager_has_prefix

  enable_security_policy              = var.enable_security_policy
  use_adaptive_protection             = var.use_adaptive_protection
  pbs_ddos_thresholds                 = var.pbs_ddos_thresholds
  pbs_security_policy_rules           = var.pbs_security_policy_rules
  enable_cloud_armor_alerts           = var.enable_cloud_armor_alerts
  enable_cloud_armor_notifications    = var.enable_cloud_armor_notifications
  cloud_armor_notification_channel_id = var.cloud_armor_notification_channel_id

  depends_on = [
    module.pbs_a_record,
  ]
}
