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

output "pbs_environment_url" {
  description = "The PBS service endpoint."
  value       = local.pbs_service_endpoint
}

output "pbs_auth_audience_url" {
  description = "The auth token target audience endpoint for the PBS."
  value       = module.pbs_auth.pbs_auth_audience_url
}

output "pbs_auth_spanner_reporting_origin_instance_name" {
  description = "The Spanner instance name for the PBS auth service."
  value       = module.pbs_auth_storage.pbs_auth_spanner_instance_name
}

output "pbs_auth_spanner_reporting_origin_database_name" {
  description = "The Spanner database name for the PBS auth service."
  value       = module.pbs_auth_storage.pbs_auth_spanner_database_name
}

output "pbs_auth_spanner_authorization_v2_table_name" {
  description = "Name of the PBS authorization V2 table"
  value       = module.pbs_auth_storage.pbs_auth_spanner_authorization_v2_table_name
}

output "pbs_service_account_email" {
  description = "The service account for the PBS."
  value       = var.pbs_service_account_email
}

output "pbs_spanner_database_name" {
  value       = module.pbs_storage.pbs_spanner_database_name
  description = "Name of the PBS Spanner database."
}

output "pbs_spanner_instance_name" {
  value       = module.pbs_storage.pbs_spanner_instance_name
  description = "Name of the PBS Spanner instance."
}

output "pbs_alternate_instance_domain_record_data" {
  value       = module.pbs_lb.pbs_alternate_instance_domain_record_data
  description = "Alternate DNS record data."
}