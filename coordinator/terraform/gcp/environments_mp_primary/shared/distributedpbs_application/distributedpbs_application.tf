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

module "distributedpbs_application" {
  source                                         = "../../../applications/distributedpbs_application"
  project                                        = var.project
  region                                         = var.region
  environment                                    = var.environment
  pbs_remote_coordinator_service_account_email   = var.pbs_remote_coordinator_service_account_email
  pbs_artifact_registry_repository_name          = var.pbs_artifact_registry_repository_name
  pbs_cloud_storage_journal_bucket_force_destroy = var.pbs_cloud_storage_journal_bucket_force_destroy
  pbs_cloud_storage_journal_bucket_versioning    = var.pbs_cloud_storage_journal_bucket_versioning
  pbs_auth_spanner_database_retention_period     = var.pbs_auth_spanner_database_retention_period
  pbs_auth_spanner_instance_processing_units     = var.pbs_auth_spanner_instance_processing_units
  pbs_auth_spanner_database_deletion_protection  = var.pbs_auth_spanner_database_deletion_protection
  pbs_spanner_database_retention_period          = var.pbs_spanner_database_retention_period
  pbs_spanner_instance_processing_units          = var.pbs_spanner_instance_processing_units
  pbs_spanner_database_deletion_protection       = var.pbs_spanner_database_deletion_protection
  auth_cloud_function_handler_path               = var.auth_cloud_function_handler_path
  pbs_auth_cloudfunction_min_instances           = var.pbs_auth_cloudfunction_min_instances
  pbs_auth_cloudfunction_max_instances           = var.pbs_auth_cloudfunction_max_instances
  pbs_auth_cloudfunction_instance_concurrency    = var.pbs_auth_cloudfunction_instance_concurrency
  pbs_auth_cloudfunction_instance_available_cpu  = var.pbs_auth_cloudfunction_instance_available_cpu
  pbs_auth_cloudfunction_memory_mb               = var.pbs_auth_cloudfunction_memory_mb
  pbs_auth_cloudfunction_timeout_seconds         = var.pbs_auth_cloudfunction_timeout_seconds
  pbs_auth_package_bucket_force_destroy          = var.pbs_auth_package_bucket_force_destroy
  pbs_auth_allowed_principals                    = var.pbs_auth_allowed_principals
  pbs_service_account_email                      = var.pbs_service_account_email
  pbs_image_tag                                  = var.pbs_image_tag
  pbs_custom_vm_tags                             = var.pbs_custom_vm_tags
  pbs_autoscaling_policy                         = var.pbs_autoscaling_policy
  pbs_main_port                                  = var.pbs_main_port
  pbs_application_environment_variables          = var.pbs_application_environment_variables
  machine_type                                   = var.machine_type
  root_volume_size_gb                            = var.root_volume_size_gb
  pbs_cloud_logging_enabled                      = var.pbs_cloud_logging_enabled
  pbs_cloud_monitoring_enabled                   = var.pbs_cloud_monitoring_enabled
  pbs_instance_allow_ssh                         = var.pbs_instance_allow_ssh
  parent_domain_project                          = var.parent_domain_project
  parent_domain_name                             = var.parent_domain_name
  service_subdomain                              = var.service_subdomain
  service_subdomain_suffix                       = var.service_subdomain_suffix
  vpc_network_id                                 = var.vpc_network_id
  vpc_subnet_id                                  = var.vpc_subnet_id
  enable_public_ip_address                       = var.enable_public_ip_address
  enable_domain_management                       = var.enable_domain_management
  enable_health_check                            = var.enable_health_check
  pbs_tls_alternate_names                        = var.pbs_tls_alternate_names
}
