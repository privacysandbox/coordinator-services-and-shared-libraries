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
  cloud_run_name = "private-key-service"
}

module "version" {
  source = "../version"
}

resource "google_service_account" "encryption_key_service_account" {
  project = var.project_id
  # Service account id has a 30 character limit
  account_id   = "${var.environment}-encryptionkeyuser"
  display_name = "Encryption Key Service Account"
}

# IAM entry for service account to read from the database
resource "google_spanner_database_iam_member" "encryption_key_service_spannerdb_iam_policy" {
  project  = var.project_id
  instance = var.spanner_instance_name
  database = var.spanner_database_name
  role     = "roles/spanner.databaseReader"
  member   = "serviceAccount:${google_service_account.encryption_key_service_account.email}"
}

# Cloud Run Service to get the encryption keys.
resource "google_cloud_run_v2_service" "private_key_service" {
  project  = var.project_id
  name     = "${var.environment}-${var.region}-${local.cloud_run_name}"
  location = var.region
  ingress  = "INGRESS_TRAFFIC_INTERNAL_LOAD_BALANCER"

  template {
    containers {
      image = var.private_key_service_image

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
        name  = "EXPORT_OTEL_METRICS"
        value = var.export_otel_metrics
      }

      resources {
        limits = {
          cpu    = var.encryption_key_service_cpus
          memory = "${var.encryption_key_service_cloudfunction_memory_mb}M"
        }
        cpu_idle          = true
        startup_cpu_boost = true
      }
    }

    scaling {
      min_instance_count = var.encryption_key_service_cloudfunction_min_instances
      max_instance_count = var.encryption_key_service_cloudfunction_max_instances
    }

    max_instance_request_concurrency = var.encryption_key_service_request_concurrency
    timeout                          = "${var.cloudfunction_timeout_seconds}s"

    labels = {
      version = lower(join("_", regexall("[a-zA-Z0-9\\-]+", module.version.version))),
      # Create a new revision if cloud_run_revision_force_replace is true. This
      # is done by applying a unique timestamp label on each deployment.
      force_new_revision_timestamp = var.cloud_run_revision_force_replace ? formatdate("YYYY-MM-DD_hh_mm_ss", timestamp()) : null,
    }

    service_account = google_service_account.encryption_key_service_account.email
  }

  custom_audiences = var.private_key_service_custom_audiences
}

# IAM entry to invoke the cloud run service.
resource "google_cloud_run_service_iam_member" "private_key_service" {
  project  = var.project_id
  location = google_cloud_run_v2_service.private_key_service.location
  service  = google_cloud_run_v2_service.private_key_service.name

  role   = "roles/run.invoker"
  member = "group:${var.allowed_operator_user_group}"
}

# IAM entry to allow encryption key cloud function to write metrics.
resource "google_project_iam_member" "encryption_key_service_monitoring_iam_policy" {
  project = var.project_id
  role    = "roles/monitoring.metricWriter"
  member  = "serviceAccount:${google_service_account.encryption_key_service_account.email}"
}
