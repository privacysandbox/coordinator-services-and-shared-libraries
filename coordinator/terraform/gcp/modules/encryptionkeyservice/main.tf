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

locals {
  cloudfunction_name_suffix            = "encryption-key-service-cloudfunction"
  cloudfunction_package_zip            = var.encryption_key_service_zip
  encryption_key_service_account_email = google_service_account.encryption_key_service_account.email
}

module "version" {
  source = "../version"
}

resource "google_service_account" "encryption_key_service_account" {
  # Service account id has a 30 character limit
  account_id   = "${var.environment}-encryptionkeyuser"
  display_name = "Encryption Key Service Account"
}

resource "google_storage_bucket_object" "encryption_key_service_package_bucket_object" {
  # Need hash in name so cloudfunction knows to redeploy when code changes
  name   = "${var.environment}_${local.cloudfunction_name_suffix}_${filemd5(local.cloudfunction_package_zip)}"
  bucket = var.package_bucket_name
  source = local.cloudfunction_package_zip
}

resource "google_cloudfunctions2_function" "encryption_key_service_cloudfunction" {
  name     = "${var.environment}-${var.region}-${local.cloudfunction_name_suffix}"
  location = var.region

  build_config {
    runtime     = "java17"
    entry_point = "com.google.scp.coordinator.keymanagement.keyhosting.service.gcp.EncryptionKeyServiceHttpFunction"
    source {
      storage_source {
        bucket = var.package_bucket_name
        object = google_storage_bucket_object.encryption_key_service_package_bucket_object.name
      }
    }
  }

  service_config {
    min_instance_count    = var.encryption_key_service_cloudfunction_min_instances
    max_instance_count    = var.encryption_key_service_cloudfunction_max_instances
    timeout_seconds       = var.cloudfunction_timeout_seconds
    available_memory      = "${var.encryption_key_service_cloudfunction_memory_mb}M"
    service_account_email = local.encryption_key_service_account_email
    ingress_settings      = "ALLOW_INTERNAL_AND_GCLB"
    environment_variables = {
      PROJECT_ID       = var.project_id
      SPANNER_INSTANCE = var.spanner_instance_name
      SPANNER_DATABASE = var.spanner_database_name
      VERSION          = module.version.version
      LOG_EXECUTION_ID = "true"
    }
  }

  labels = {
    environment = var.environment
  }

  lifecycle {
    ignore_changes = [
      build_config[0].docker_repository
    ]
  }
}

# IAM entry for service account to read from the database
resource "google_spanner_database_iam_member" "encryption_key_service_spannerdb_iam_policy" {
  instance = var.spanner_instance_name
  database = var.spanner_database_name
  role     = "roles/spanner.databaseReader"
  member   = "serviceAccount:${local.encryption_key_service_account_email}"
}

# IAM entry to invoke the function. Gen 2 cloud functions need CloudRun permissions.
resource "google_cloud_run_service_iam_member" "encryption_key_service_iam_policy" {
  project  = var.project_id
  location = google_cloudfunctions2_function.encryption_key_service_cloudfunction.location
  service  = google_cloudfunctions2_function.encryption_key_service_cloudfunction.name

  role   = "roles/run.invoker"
  member = "group:${var.allowed_operator_user_group}"
}
