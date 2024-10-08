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

# Must be updated.
project = "<project-id>"
region  = "<deployment-region>"

# This must match the environment name in distributedpbs_base.
# Referenced in comments below as <environment>.
environment = "demo-b"

# This must be updated to the output `pbs_service_account_email` of the remote coordinator, known after its initial deployment.
# If the service account does not yet exist, only the empty string will be accepted.
pbs_remote_coordinator_service_account_email = ""

## Uncomment one of the configuration blocks below depending on your type of deployment.

## BEGIN - development default configuration values.
# # It can be helpful to reserve GCP resources like CPUs in advance.
# machine_type        = "e2-standard-16"
# root_volume_size_gb = "512"
# # Both of these must be true to allow ssh
# pbs_instance_allow_ssh   = true
# enable_public_ip_address = true
# # Allows for the destruction of buckets when they are not empty.
# pbs_cloud_storage_journal_bucket_force_destroy = true
# pbs_auth_package_bucket_force_destroy          = true
# pbs_cloud_storage_journal_bucket_versioning    = true
# # Processing units can get expensive! Only use what you need!
# # See https://cloud.google.com/spanner/docs/compute-capacity
# pbs_auth_spanner_instance_processing_units = 200
# pbs_spanner_instance_processing_units      = 200
# # Will prevent `terraform destroy` from succeeding.
# pbs_auth_spanner_database_deletion_protection = false
# pbs_spanner_database_deletion_protection      = false
# # The max duration for retention is seven days.
# pbs_auth_spanner_database_retention_period = "7d"
# pbs_spanner_database_retention_period      = "7d"
# # Affects the number of concurrent requests the auth cloud function can process.
# # See https://cloud.google.com/run/docs/configuring/max-instances
# pbs_auth_cloudfunction_min_instances   = 1
# pbs_auth_cloudfunction_max_instances   = 1
## END - development default configuration values.

## BEGIN - production default configuration values.
# # It can be helpful to reserve GCP resources like CPUs in advance.
# machine_type        = "n2-highmem-128"
# root_volume_size_gb = "4096"
# # Both of these must be true to allow ssh
# pbs_instance_allow_ssh   = false
# enable_public_ip_address = false
# # Allows destruction of buckets when they are not empty.
# pbs_cloud_storage_journal_bucket_force_destroy = false
# pbs_auth_package_bucket_force_destroy          = false
# pbs_cloud_storage_journal_bucket_versioning    = true
# # Processing units can get expensive! Only use what you need!
# # See https://cloud.google.com/spanner/docs/compute-capacity
# pbs_auth_spanner_instance_processing_units = 2000
# pbs_spanner_instance_processing_units      = 8000
# # Will prevent `terraform destroy` from succeeding.
# pbs_auth_spanner_database_deletion_protection = true
# pbs_spanner_database_deletion_protection      = true
# # The max duration for retention is seven days.
# pbs_auth_spanner_database_retention_period = "7d"
# pbs_spanner_database_retention_period      = "7d"
# # Affects the number of concurrent requests the auth cloud function can process.
# # See https://cloud.google.com/run/docs/configuring/max-instances
# pbs_auth_cloudfunction_min_instances   = 1
# pbs_auth_cloudfunction_max_instances   = 10
## END - production default configuration values.

# Domain management:
# Environment name domain name will be:
# <service_subdomain>-<environment>.<parent_domain_name>
# With the values below:
# mp-pbs-demo-a.gcp.coordinator.dev
# Note: Enabling domain management prevents redeploying without domain management.
enable_domain_management = false
service_subdomain        = "mp-pbs"

parent_domain_name    = "<domain name from domainrecordsetup>"
parent_domain_project = "<project_id from domainrecordsetup>"

pbs_application_environment_variables = [
  ##### Secondary deployment information

  #### These values here must be updated. If they are known at deployment time, then they
  #### can be provided, otherwise, they can be added in a subsequent terraform apply.

  ## Uncomment one of the configuration blocks below depending on your type of deployment.

  ## BEGIN - development default configuration values.
  # {
  #   name  = "google_scp_pbs_async_executor_threads_count"
  #   value = "16"
  # },
  # {
  #   name  = "google_scp_pbs_io_async_executor_threads_count"
  #   value = "128"
  # },
  # {
  #   name  = "google_scp_core_http2server_threads_count"
  #   value = "80"
  # },
  # {
  #   name = "google_scp_pbs_remote_host_address"
  #   # This must be updated to the output `pbs_environment_url` of the remote coordinator, known after its initial deployment.
  #   value = "<remote-coordinator-environment-url>"
  # },
  # {
  #   name = "google_scp_pbs_remote_auth_endpoint"
  #   # This must be updated to the output `pbs_auth_audience_url` of the remote coordinator, known after its initial deployment.
  #   value = "<remote-coordinator-auth-audience-url>"
  # },
  # {
  #   name = "google_scp_pbs_remote_cloud_region"
  #   # This must be updated to the region where the remote coordinator is deployed.
  #   value = "<remote-coordinator-region>"
  # },
  ## END - development default configuration values.

  ## BEGIN - production default configuration values.
  # {
  #   name  = "google_scp_pbs_async_executor_threads_count"
  #   value = "128"
  # },
  # {
  #   name  = "google_scp_pbs_io_async_executor_threads_count"
  #   value = "1024"
  # },
  # {
  #   name  = "google_scp_core_http2server_threads_count"
  #   value = "640"
  # },
  # {
  #   name = "google_scp_pbs_remote_host_address"
  #   # This must be updated to the output `pbs_environment_url` of the remote coordinator, known after its initial deployment.
  #   value = "<remote-coordinator-environment-url>"
  # },
  # {
  #   name = "google_scp_pbs_remote_auth_endpoint"
  #   # This must be updated to the output `pbs_auth_audience_url` of the remote coordinator, known after its initial deployment.
  #   value = "<remote-coordinator-auth-audience-url>"
  # },
  # {
  #   name = "google_scp_pbs_remote_cloud_region"
  #   # This must be updated to the region where the remote coordinator is deployed.
  #   value = "<remote-coordinator-region>"
  # },
  ## END - production default configuration values.
]

# Path to the auth lambda handler and its requirements, relative to this module.
auth_cloud_function_handler_path = "../../../dist/auth_cloud_function_handler.zip"

# This must be updated to the allowed operators group.
pbs_auth_allowed_principals = ["group:<group-allowed-operators-email>"]
