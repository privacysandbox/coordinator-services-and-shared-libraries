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
      version = ">= 5.37.0"
    }
  }
}

locals {
  key_storage_package_zip = var.key_storage_service_zip

  cloud_run_name = "key-storage-service"
  # Generates a unique suffix for Cloud Run service revisions based on the
  # container image url and optional timestamp. The timestamp part allows
  # forcing the creation of a new revision when cloud_run_revision_force_replace
  # is set to true.
  cloud_run_revision_suffix = var.use_cloud_run ? substr(
    sha256(
      format(
        "%s%s",
        var.key_storage_service_image,
        var.cloud_run_revision_force_replace ? timestamp() : ""
      )
    ), 0, 8
  ) : ""
}

module "version" {
  source = "../version"
}

resource "google_service_account" "key_storage_service_account" {
  project = var.project_id
  # Service account id has a 30 character limit
  account_id   = "${var.environment}-keystorageuser"
  display_name = "KeyStorage Service Account"
}

resource "google_storage_bucket_object" "key_storage_archive" {
  # Need hash in name so cloudfunction knows to redeploy when code changes
  name   = "${var.environment}_key_storage_${filemd5(local.key_storage_package_zip)}"
  bucket = var.package_bucket_name
  source = local.key_storage_package_zip
}

resource "google_cloudfunctions2_function" "key_storage_cloudfunction" {
  count = !var.use_cloud_run ? 1 : 0

  project     = var.project_id
  name        = "${var.environment}-${var.region}-${var.key_storage_cloudfunction_name}"
  location    = var.region
  description = "Cloud Function for key storage service"

  build_config {
    runtime     = "java17"
    entry_point = "com.google.scp.coordinator.keymanagement.keystorage.service.gcp.KeyStorageServiceHttpFunction"
    source {
      storage_source {
        bucket = var.package_bucket_name
        object = google_storage_bucket_object.key_storage_archive.name
      }
    }
  }

  service_config {
    min_instance_count    = var.key_storage_service_cloudfunction_min_instances
    max_instance_count    = var.key_storage_service_cloudfunction_max_instances
    timeout_seconds       = var.cloudfunction_timeout_seconds
    available_memory      = "${var.key_storage_cloudfunction_memory}M"
    service_account_email = google_service_account.key_storage_service_account.email
    ingress_settings      = "ALLOW_INTERNAL_AND_GCLB"
    environment_variables = {
      PROJECT_ID          = var.project_id
      GCP_KMS_URI         = "gcp-kms://${var.key_encryption_key_id}"
      SPANNER_INSTANCE    = var.spanner_instance_name
      SPANNER_DATABASE    = var.spanner_database_name
      VERSION             = module.version.version
      LOG_EXECUTION_ID    = "true"
      EXPORT_OTEL_METRICS = var.export_otel_metrics
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

# IAM entry for key storage service account to use the database
resource "google_spanner_database_iam_member" "keydb_iam_policy" {
  project  = var.project_id
  instance = var.spanner_instance_name
  database = var.spanner_database_name
  role     = "roles/spanner.databaseUser"
  member   = "serviceAccount:${google_service_account.key_storage_service_account.email}"
}

# IAM entry to invoke the function. Gen 2 cloud functions need CloudRun permissions.
resource "google_cloud_run_service_iam_member" "cloud_function_iam_policy" {
  for_each = !var.use_cloud_run ? setunion(var.allowed_wip_iam_principals, var.allowed_wip_user_group != null ? ["group:${var.allowed_wip_user_group}"] : []) : toset([])

  project  = var.project_id
  location = google_cloudfunctions2_function.key_storage_cloudfunction[0].location
  service  = google_cloudfunctions2_function.key_storage_cloudfunction[0].name

  role   = "roles/run.invoker"
  member = each.key
}

# Cloud Run Service for Key Storage Service.
resource "google_cloud_run_v2_service" "key_storage_service" {
  count = var.use_cloud_run ? 1 : 0

  name     = "${var.environment}-${var.region}-${local.cloud_run_name}"
  location = var.region
  ingress  = "INGRESS_TRAFFIC_INTERNAL_LOAD_BALANCER"

  template {
    containers {
      image = var.key_storage_service_image

      env {
        name  = "PROJECT_ID"
        value = var.project_id
      }
      env {
        name  = "SPANNER_INSTANCE"
        value = var.spanner_instance_name
      }
      env {
        name  = "SPANNER_DATABASE"
        value = var.spanner_database_name
      }
      env {
        name  = "GCP_KMS_URI"
        value = "gcp-kms://${var.key_encryption_key_id}"
      }
      env {
        name  = "EXPORT_OTEL_METRICS"
        value = var.export_otel_metrics
      }

      resources {
        limits = {
          memory = "${var.key_storage_cloudfunction_memory}M"
        }
      }
    }

    scaling {
      min_instance_count = var.key_storage_service_cloudfunction_min_instances
      max_instance_count = var.key_storage_service_cloudfunction_max_instances
    }

    timeout = "${var.cloudfunction_timeout_seconds}s"

    labels = {
      version = lower(join("_", regexall("[a-zA-Z0-9\\-]+", module.version.version))),
    }

    service_account = google_service_account.key_storage_service_account.email
    revision        = "${var.environment}-${var.region}-${local.cloud_run_name}-${local.cloud_run_revision_suffix}"
  }

  custom_audiences = var.key_storage_service_custom_audiences
}

# IAM entry to invoke the cloud run service.
resource "google_cloud_run_service_iam_member" "key_storage_service" {
  for_each = var.use_cloud_run ? setunion(var.allowed_wip_iam_principals, var.allowed_wip_user_group != null ? ["group:${var.allowed_wip_user_group}"] : []) : toset([])

  project  = var.project_id
  location = google_cloud_run_v2_service.key_storage_service[0].location
  service  = google_cloud_run_v2_service.key_storage_service[0].name

  role   = "roles/run.invoker"
  member = each.key
}

# IAM entry to allow function to encrypt and decrypt using KMS
resource "google_kms_crypto_key_iam_member" "kms_iam_policy" {
  crypto_key_id = var.key_encryption_key_id
  role          = "roles/cloudkms.cryptoKeyEncrypterDecrypter"
  member        = "serviceAccount:${google_service_account.key_storage_service_account.email}"
}

# IAM entry to allow key storage cloud function to write metrics.
resource "google_project_iam_member" "keystorage_monitoring_iam_policy" {
  project = var.project_id
  role    = "roles/monitoring.metricWriter"
  member  = "serviceAccount:${google_service_account.key_storage_service_account.email}"
}
