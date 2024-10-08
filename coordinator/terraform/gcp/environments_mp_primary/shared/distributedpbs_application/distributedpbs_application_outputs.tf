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

output "pbs_environment_url" {
  description = "The PBS service endpoint."
  value       = module.distributedpbs_application.pbs_environment_url
}

output "pbs_auth_audience_url" {
  description = "The auth token target audience endpoint for the PBS."
  value       = module.distributedpbs_application.pbs_auth_audience_url
}

output "pbs_auth_spanner_reporting_origin_instance_name" {
  description = "The Spanner instance name for the PBS auth service."
  value       = module.distributedpbs_application.pbs_auth_spanner_reporting_origin_instance_name
}

output "pbs_auth_spanner_reporting_origin_database_name" {
  description = "The Spanner database name for the PBS auth service."
  value       = module.distributedpbs_application.pbs_auth_spanner_reporting_origin_database_name
}

output "pbs_auth_spanner_authorization_v2_table_name" {
  description = "Name of the PBS authorization V2 table"
  value       = module.distributedpbs_application.pbs_auth_spanner_authorization_v2_table_name
}

output "pbs_service_account_email" {
  description = "The service account for this PBS coordinator."
  value       = module.distributedpbs_application.pbs_service_account_email
}

output "pbs_remote_coordinator_service_account_email" {
  description = "The service account for the remote PBS coordinator."
  value       = var.pbs_remote_coordinator_service_account_email
}

output "pbs_spanner_database_name" {
  value       = module.distributedpbs_application.pbs_spanner_database_name
  description = "Name of the PBS Spanner database."
}

output "pbs_spanner_instance_name" {
  value       = module.distributedpbs_application.pbs_spanner_instance_name
  description = "Name of the PBS Spanner instance."
}

output "pbs_alternate_instance_domain_record_data" {
  value       = module.distributedpbs_application.pbs_alternate_instance_domain_record_data
  description = "Alternate DNS record data."
}
