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
  cloud_run_name = "key-storage-service"
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

# IAM entry for key storage service account to use the database
resource "google_spanner_database_iam_member" "keydb_iam_policy" {
  project  = var.project_id
  instance = var.spanner_instance_name
  database = var.spanner_database_name
  role     = "roles/spanner.databaseUser"
  member   = "serviceAccount:${google_service_account.key_storage_service_account.email}"
}

# Cloud Run Service for Key Storage Service.
resource "google_cloud_run_v2_service" "key_storage_service" {
  project  = var.project_id
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
      env {
        name  = "AWS_XC_ENABLED"
        value = var.aws_xc_enabled ? "true" : null
      }
      env {
        name  = "AWS_KMS_URI"
        value = var.aws_xc_enabled ? "aws-kms://${var.aws_kms_key_encryption_key_arn}" : null
      }
      env {
        name  = "AWS_KMS_ROLE_ARN"
        value = var.aws_xc_enabled ? var.aws_kms_key_encryption_key_role_arn : null
      }
      env {
        name  = "AWS_KEY_SYNC_ENABLED"
        value = var.aws_key_sync_enabled ? "true" : null
      }
      env {
        name  = "AWS_KEY_SYNC_ROLE_ARN"
        value = var.aws_key_sync_enabled ? var.aws_key_sync_role_arn : null
      }
      env {
        name  = "AWS_KEY_SYNC_KMS_KEY_URI"
        value = var.aws_key_sync_enabled ? "aws-kms://${var.aws_key_sync_kms_key_uri}" : null
      }
      env {
        name  = "AWS_KEY_SYNC_KEYDB_REGION"
        value = var.aws_key_sync_enabled ? var.aws_key_sync_keydb_region : null
      }
      env {
        name  = "AWS_KEY_SYNC_KEYDB_TABLE_NAME"
        value = var.aws_key_sync_enabled ? var.aws_key_sync_keydb_table_name : null
      }

      resources {
        limits = {
          cpu    = 1
          memory = "${var.key_storage_cloudfunction_memory}M"
        }
        cpu_idle = true
      }
    }

    scaling {
      min_instance_count = var.key_storage_service_cloudfunction_min_instances
      max_instance_count = var.key_storage_service_cloudfunction_max_instances
    }

    timeout = "${var.cloudfunction_timeout_seconds}s"

    labels = {
      version = lower(join("_", regexall("[a-zA-Z0-9\\-]+", module.version.version))),
      # Create a new revision if cloud_run_revision_force_replace is true. This
      # is done by applying a unique timestamp label on each deployment.
      force_new_revision_timestamp = var.cloud_run_revision_force_replace ? formatdate("YYYY-MM-DD_hh_mm_ss", timestamp()) : null,
    }

    service_account = google_service_account.key_storage_service_account.email
  }

  custom_audiences = var.key_storage_service_custom_audiences
}

# IAM entry to invoke the cloud run service.
resource "google_cloud_run_service_iam_member" "key_storage_service" {
  for_each = setunion(var.allowed_wip_iam_principals, var.allowed_wip_user_group != null ? ["group:${var.allowed_wip_user_group}"] : [])

  project  = var.project_id
  location = google_cloud_run_v2_service.key_storage_service.location
  service  = google_cloud_run_v2_service.key_storage_service.name

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
