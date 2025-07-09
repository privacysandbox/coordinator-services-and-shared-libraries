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
  cloudfunction_name_suffix  = "pbs-auth-cloudfunction"
  pbs_auth_svc_account_email = google_service_account.pbs_auth_svc_account.email
}

resource "google_service_account" "pbs_auth_svc_account" {
  # Service account ID has a 30 character limit and our environment names are
  # allowed to be up to 18 characters
  account_id   = "${var.environment}-pbsauthacnt"
  display_name = "PBS Auth Service Account"
}

resource "random_id" "gcs_bucket_random_part" {
  byte_length = 8
}

resource "google_storage_bucket" "pbs_auth_package" {
  # Bucket names must be globally unique so we add a random part to the name.
  # Total must be max 63 characters
  name                        = "${var.environment}_pbs_auth_package_${random_id.gcs_bucket_random_part.hex}"
  count                       = var.pbs_auth_package_bucket == null ? 1 : 0
  location                    = var.region
  force_destroy               = var.pbs_auth_package_bucket_force_destroy
  uniform_bucket_level_access = true
}

resource "google_storage_bucket_object" "pbs_auth_package_bucket_object" {
  # Need hash in name so cloudfunction knows to redeploy when code changes
  name   = "${var.environment}_${local.cloudfunction_name_suffix}_${filesha256(var.auth_cloud_function_handler_path)}"
  count  = var.pbs_auth_package_path == null ? 1 : 0
  bucket = var.pbs_auth_package_bucket != null ? var.pbs_auth_package_bucket : google_storage_bucket.pbs_auth_package[0].name
  # This is used by local and CI deployments.
  # tflint-ignore: opa_deny_gcs_local_source
  source = var.auth_cloud_function_handler_path

  lifecycle {
    precondition {
      condition     = var.auth_cloud_function_handler_path != null
      error_message = "variable 'auth_cloud_function_handler_path' must be set"
    }
  }
}

resource "google_cloudfunctions2_function" "pbs_auth_cloudfunction" {
  name     = "${var.environment}-${var.region}-${local.cloudfunction_name_suffix}"
  location = var.region

  build_config {
    runtime     = "python310"
    entry_point = "function_handler"
    source {
      storage_source {
        bucket = var.pbs_auth_package_bucket != null ? var.pbs_auth_package_bucket : google_storage_bucket.pbs_auth_package[0].name
        object = var.pbs_auth_package_path != null ? var.pbs_auth_package_path : google_storage_bucket_object.pbs_auth_package_bucket_object[0].name
      }
    }
  }

  service_config {
    min_instance_count               = var.pbs_auth_cloudfunction_min_instances
    max_instance_count               = var.pbs_auth_cloudfunction_max_instances
    max_instance_request_concurrency = var.pbs_auth_cloudfunction_instance_concurrency
    available_cpu                    = var.pbs_auth_cloudfunction_instance_available_cpu
    available_memory                 = "${var.pbs_auth_cloudfunction_memory_mb}M"
    timeout_seconds                  = var.pbs_auth_cloudfunction_timeout_seconds

    service_account_email = local.pbs_auth_svc_account_email
    environment_variables = {
      PROJECT_ID                                = var.project_id
      REPORTING_ORIGIN_AUTH_SPANNER_INSTANCE_ID = var.pbs_auth_spanner_instance_name
      REPORTING_ORIGIN_AUTH_SPANNER_DATABASE_ID = var.pbs_auth_spanner_database_name
      AUTH_V2_SPANNER_INSTANCE_ID               = var.pbs_auth_spanner_instance_name
      AUTH_V2_SPANNER_DATABASE_ID               = var.pbs_auth_spanner_database_name
      AUTH_V2_SPANNER_TABLE_NAME                = var.pbs_auth_v2_spanner_table_name
      LOG_EXECUTION_ID                          = true
    }

    ingress_settings = var.limit_traffic_to_internal_and_lb ? "ALLOW_INTERNAL_AND_GCLB" : "ALLOW_ALL"
  }

  labels = {
    environment = var.environment
  }
}

# IAM entry for cloud function service account to read from the database
resource "google_spanner_database_iam_member" "pbs_auth_spannerdb_iam_policy" {
  project  = var.project_id
  instance = var.pbs_auth_spanner_instance_name
  database = var.pbs_auth_spanner_database_name
  role     = "roles/spanner.databaseReader"
  member   = "serviceAccount:${local.pbs_auth_svc_account_email}"
}

# IAM entries to invoke the function. Gen 2 cloud functions need CloudRun permissions.
resource "google_cloud_run_service_iam_member" "pbs_auth_execution_iam_policy" {
  project  = var.project_id
  location = google_cloudfunctions2_function.pbs_auth_cloudfunction.location
  service  = google_cloudfunctions2_function.pbs_auth_cloudfunction.name

  role = "roles/run.invoker"

  for_each = toset(var.pbs_auth_allowed_principals)
  member   = each.value
}
