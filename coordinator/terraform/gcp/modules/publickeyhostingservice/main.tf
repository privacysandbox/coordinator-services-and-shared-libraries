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
  cloud_run_name = "public-key-service"

  # All configurations to the cloud run service MUST be included in here
  service_config = {
    image                            = var.public_key_service_image
    cpu                              = var.get_public_key_cpus
    memory                           = "${var.get_public_key_cloudfunction_memory_mb}M"
    min_instance_count               = var.get_public_key_cloudfunction_min_instances
    max_instance_count               = var.get_public_key_cloudfunction_max_instances
    max_instance_request_concurrency = var.get_public_key_request_concurrency
    timeout                          = "${var.cloudfunction_timeout_seconds}s"
    service_account                  = google_service_account.public_key_service_account.email
    env = {
      APPLICATION_NAME    = var.application_name
      PROJECT_ID          = var.project_id
      SPANNER_INSTANCE    = var.spanner_instance_name
      SPANNER_DATABASE    = var.spanner_database_name
      EXPORT_OTEL_METRICS = var.export_otel_metrics
    }
    labels = {
      version : lower(join("_", regexall("[a-zA-Z0-9\\-]+", module.version.version)))
      force_new_revision_timestamp : var.cloud_run_revision_force_replace ? formatdate("YYYY-MM-DD_hh_mm_ss", timestamp()) : null
    }
    cpu_idle          = true
    startup_cpu_boost = true
  }

  # Generate a unique suffix by hasing the cloud run configuration.
  revision_canary_suffix = substr(sha256(jsonencode(local.service_config)), 0, 8)
  revision_canary_map = {
    for region in var.regions : region => "${var.environment}-${region}-${local.cloud_run_name}-${local.revision_canary_suffix}"
  }

  # Retrieve the name of the stable revision which tagged with "stable".
  # It iterates through the traffic statuses to filters for the entry with tag
  # "stable" and extracts its revision name. The 'try' function handles the edge
  # case where no revision is tagged as "stable", setting the value to null.
  revision_stable_map = {
    for region in var.regions : region => try(
      [
        for traffic in data.google_cloud_run_v2_service.public_key_service_observed[region].traffic_statuses :
        traffic.revision
        if traffic.tag == "stable"
      ][0],
      null
    )
  }
}

module "version" {
  source = "../version"
}

# Data resource to retrieve information from the Cloud Run service.
data "google_cloud_run_v2_service" "public_key_service_observed" {
  for_each = var.public_key_service_canary_percent < 100 ? var.regions : []

  project  = var.project_id
  name     = "${var.environment}-${each.key}-${local.cloud_run_name}"
  location = each.key
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

  # All changes to the template must be made via local.service_config.
  template {
    containers {
      image = local.service_config.image

      dynamic "env" {
        for_each = toset(keys(local.service_config.env))
        content {
          name  = env.value
          value = local.service_config.env[env.value]
        }
      }

      resources {
        limits = {
          cpu    = local.service_config.cpu
          memory = local.service_config.memory
        }
        cpu_idle          = local.service_config.cpu_idle
        startup_cpu_boost = local.service_config.startup_cpu_boost
      }
    }

    scaling {
      min_instance_count = local.service_config.min_instance_count
      max_instance_count = local.service_config.max_instance_count
    }

    revision                         = local.revision_canary_map[each.key]
    labels                           = local.service_config.labels
    max_instance_request_concurrency = local.service_config.max_instance_request_concurrency
    service_account                  = local.service_config.service_account
    timeout                          = local.service_config.timeout
  }

  traffic {
    percent  = var.public_key_service_canary_percent
    revision = local.revision_canary_map[each.key]
    tag      = var.public_key_service_canary_percent < 100 ? "canary" : "stable"
    type     = "TRAFFIC_TARGET_ALLOCATION_TYPE_REVISION"
  }

  dynamic "traffic" {
    for_each = var.public_key_service_canary_percent < 100 ? [1] : []
    content {
      percent  = 100 - var.public_key_service_canary_percent
      revision = local.revision_stable_map[each.key]
      tag      = "stable"
      type     = "TRAFFIC_TARGET_ALLOCATION_TYPE_REVISION"
    }
  }

  lifecycle {
    precondition {
      condition     = !(var.public_key_service_canary_percent < 100 && local.revision_canary_map[each.key] == local.revision_stable_map[each.key])
      error_message = <<-EOT
        The Cloud Run configuration for Public Key Service in region "${each.key}" is already deployed as revision "${local.revision_canary_map[each.key]}" with 100% traffic.
        Deploying again with partial traffic (${var.public_key_service_canary_percent}%) is not applicable.
      EOT
    }
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
