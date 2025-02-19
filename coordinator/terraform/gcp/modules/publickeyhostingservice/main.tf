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
  cloudfunction_name_suffix = "get-public-key-cloudfunction"
  cloudfunction_package_zip = var.get_public_key_service_zip
  use_cloud_function        = local.cloudfunction_package_zip != null
  cloud_run_name            = "public-key-service"
}

module "version" {
  source = "../version"
}

resource "google_storage_bucket_object" "get_public_key_package_bucket_object" {
  # Need hash in name so cloudfunction knows to redeploy when code changes
  count  = local.use_cloud_function ? 1 : 0
  name   = "${var.environment}_${local.cloudfunction_name_suffix}_${filesha256(local.cloudfunction_package_zip)}"
  bucket = var.package_bucket_name
  source = local.cloudfunction_package_zip
}

# One service account for multiple public key service locations
resource "google_service_account" "public_key_service_account" {
  project = var.project_id
  # Service account id has a 30 character limit
  account_id   = "${var.environment}-pubkeyuser"
  display_name = "Public Key Service Account"
}

# IAM entry for service account to read from the database
resource "google_spanner_database_iam_member" "get_public_key_spannerdb_iam_policy" {
  project  = var.project_id
  instance = var.spanner_instance_name
  database = var.spanner_database_name
  role     = "roles/spanner.databaseReader"
  member   = "serviceAccount:${google_service_account.public_key_service_account.email}"
}

resource "google_cloudfunctions2_function" "get_public_key_cloudfunction" {
  for_each = local.use_cloud_function ? var.regions : []

  project  = var.project_id
  name     = "${var.environment}-${each.key}-${local.cloudfunction_name_suffix}"
  location = each.key

  build_config {
    runtime     = "java17"
    entry_point = "com.google.scp.coordinator.keymanagement.keyhosting.service.gcp.PublicKeyServiceHttpFunction"
    source {
      storage_source {
        bucket = var.package_bucket_name
        object = google_storage_bucket_object.get_public_key_package_bucket_object[0].name
      }
    }
  }

  service_config {
    min_instance_count               = var.get_public_key_cloudfunction_min_instances
    max_instance_count               = var.get_public_key_cloudfunction_max_instances
    max_instance_request_concurrency = var.get_public_key_request_concurrency
    available_cpu                    = var.get_public_key_cpus
    timeout_seconds                  = var.cloudfunction_timeout_seconds
    available_memory                 = "${var.get_public_key_cloudfunction_memory_mb}M"
    service_account_email            = google_service_account.public_key_service_account.email
    ingress_settings                 = "ALLOW_INTERNAL_AND_GCLB"
    environment_variables = {
      PROJECT_ID          = var.project_id
      SPANNER_INSTANCE    = var.spanner_instance_name
      SPANNER_DATABASE    = var.spanner_database_name
      APPLICATION_NAME    = var.application_name
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

# IAM entry to invoke the function. Gen 2 cloud functions need CloudRun permissions.
resource "google_cloud_run_service_iam_member" "get_public_key_iam_policy" {
  for_each = local.use_cloud_function ? google_cloudfunctions2_function.get_public_key_cloudfunction : {}

  project  = var.project_id
  location = each.value.location
  service  = each.value.name

  role = "roles/run.invoker"
  #TODO: Update so that only load balancer can invoke
  member = "allUsers"
}

# IAM entry to allow public key cloud function to write metrics.
resource "google_project_iam_member" "get_public_key_service_monitoring_iam_policy" {
  project = var.project_id
  role    = "roles/monitoring.metricWriter"
  member  = "serviceAccount:${google_service_account.public_key_service_account.email}"
}

# Cloud Run Service to serve public keys.
resource "google_cloud_run_v2_service" "public_key_service" {
  for_each = var.regions

  project  = var.project_id
  name     = "${var.environment}-${each.key}-${local.cloud_run_name}"
  location = each.key
  ingress  = "INGRESS_TRAFFIC_INTERNAL_LOAD_BALANCER"

  template {
    containers {
      image = var.public_key_service_image

      env {
        name  = "APPLICATION_NAME"
        value = var.application_name
      }
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
          cpu    = var.get_public_key_cpus
          memory = "${var.get_public_key_cloudfunction_memory_mb}M"
        }
      }
    }

    scaling {
      min_instance_count = var.get_public_key_cloudfunction_min_instances
      max_instance_count = var.get_public_key_cloudfunction_max_instances
    }

    max_instance_request_concurrency = var.get_public_key_request_concurrency
    timeout                          = "${var.cloudfunction_timeout_seconds}s"

    labels = {
      version = lower(join("_", regexall("[a-zA-Z0-9\\-]+", module.version.version))),
      # Create a new revision if cloud_run_revision_force_replace is true. This
      # is done by applying a unique timestamp label on each deployment.
      force_new_revision_timestamp = var.cloud_run_revision_force_replace ? formatdate("YYYY-MM-DD_hh_mm_ss", timestamp()) : null,
    }

    service_account = google_service_account.public_key_service_account.email
  }
}

# IAM entry to invoke the cloud run service.
resource "google_cloud_run_service_iam_member" "public_key_service" {
  for_each = google_cloud_run_v2_service.public_key_service

  project  = var.project_id
  location = each.value.location
  service  = each.value.name

  role   = "roles/run.invoker"
  member = "allUsers"
}
