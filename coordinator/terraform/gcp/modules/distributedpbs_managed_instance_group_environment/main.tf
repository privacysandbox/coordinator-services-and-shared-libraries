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
  container_certificate_path = "/opt/google/ssl/self-signed-certs"

  pbs_auth_endpoint = var.enable_domain_management ? "${var.pbs_domain}/v1/auth" : var.pbs_auth_audience_url
  pbs_image_cr      = var.pbs_image_override != null ? var.pbs_image_override : "${var.region}-docker.pkg.dev/${var.project_id}/${var.pbs_artifact_registry_repository_name}/pbs-cloud-run-image:${var.pbs_image_tag}"
  pbs_application_environment_variables_base = {
    google_scp_gcp_project_id    = var.project_id,
    google_scp_core_cloud_region = var.region,
    # This is required for Gcp Metric as the default aggregated metric interval
    # value is 1000, which exceeds Gcp custom metric quota limit (one data
    # point per 5 seconds).
    google_scp_aggregated_metric_interval_ms                              = "5000",
    google_scp_pbs_journal_service_bucket_name                            = var.pbs_cloud_storage_journal_bucket_name,
    google_scp_spanner_instance_name                                      = var.pbs_spanner_instance_name,
    google_scp_spanner_database_name                                      = var.pbs_spanner_database_name,
    google_scp_pbs_budget_key_table_name                                  = var.pbs_spanner_budget_key_table_name,
    google_scp_pbs_partition_lock_table_name                              = var.pbs_spanner_partition_lock_table_name,
    google_scp_pbs_auth_endpoint                                          = local.pbs_auth_endpoint,
    google_scp_pbs_host_port                                              = var.main_port,
    google_scp_pbs_external_exposed_host_port                             = var.main_port,
    google_scp_pbs_http2_server_private_key_file_path                     = "${local.container_certificate_path}/privatekey.pem",
    google_scp_pbs_http2_server_certificate_file_path                     = "${local.container_certificate_path}/public.crt",
    google_scp_pbs_partition_lease_duration_in_seconds                    = "5",
    google_scp_pbs_async_executor_queue_size                              = "100000000",
    google_scp_pbs_io_async_executor_queue_size                           = "100000000",
    google_scp_pbs_transaction_manager_capacity                           = "100000000",
    google_scp_pbs_journal_service_partition_name                         = "00000000-0000-0000-0000-000000000000",
    google_scp_pbs_host_address                                           = "0.0.0.0",
    google_scp_pbs_remote_claimed_identity                                = "remote-coordinator.com",
    google_scp_pbs_multi_instance_mode_disabled                           = "true"
    google_scp_transaction_manager_skip_duplicate_transaction_in_recovery = "true",
    google_scp_pbs_relaxed_consistency_enabled                            = "true",
    google_scp_otel_enabled                                               = "true",
    google_scp_otel_metric_export_interval_msec                           = "60000",
    google_scp_otel_metric_export_timeout_msec                            = "50000",
    OTEL_METRICS_EXPORTER                                                 = "googlecloud",
  }
  pbs_cloud_run_environment_variables = {
    google_scp_pbs_http2_server_use_tls            = "false"
    google_scp_pbs_log_provider                    = "StdoutLogProvider"
    google_scp_pbs_container_type                  = "cloud_run"
    google_scp_pbs_async_executor_threads_count    = "8"
    google_scp_pbs_io_async_executor_threads_count = "16"
    google_scp_core_http2server_threads_count      = "40"
  }
  pbs_application_environment_variables_child = { for item in var.pbs_application_environment_variables :
    item.name => item.value
  }

  # The environment variables in child terraform config will override the base
  # terraform config. Base terraform config is define in this file.
  #
  # Quote from:
  # https://developer.hashicorp.com/terraform/language/functions/merge
  # "If more than one given map or object defines the same key or attribute,
  # then the one that is later in the argument sequence takes precedence."

  pbs_cloud_run_environment_variables_merged = merge(local.pbs_application_environment_variables_base, local.pbs_cloud_run_environment_variables, local.pbs_application_environment_variables_child)
}

resource "google_artifact_registry_repository_iam_member" "pbs_artifact_registry_iam_read" {
  project    = var.project_id
  location   = var.region
  repository = var.pbs_artifact_registry_repository_name
  role       = "roles/artifactregistry.reader"
  member     = "serviceAccount:${var.pbs_service_account_email}"
}

resource "google_storage_bucket_iam_member" "pbs_cloud_storage_iam_admin" {
  bucket = var.pbs_cloud_storage_journal_bucket_name
  role   = "roles/storage.objectAdmin"
  member = "serviceAccount:${var.pbs_service_account_email}"
}

resource "google_spanner_database_iam_member" "pbs_spanner_iam_user" {
  project  = var.project_id
  instance = var.pbs_spanner_instance_name
  database = var.pbs_spanner_database_name
  role     = "roles/spanner.databaseUser"
  member   = "serviceAccount:${var.pbs_service_account_email}"
}

resource "google_project_iam_member" "pbs_logging_iam_writer" {
  project = var.project_id
  role    = "roles/logging.logWriter"
  member  = "serviceAccount:${var.pbs_service_account_email}"
}

resource "google_project_iam_member" "pbs_metric_iam_writer" {
  project = var.project_id
  role    = "roles/monitoring.metricWriter"
  member  = "serviceAccount:${var.pbs_service_account_email}"
}

resource "google_project_iam_member" "pbs_compute_iam_viewer" {
  project = var.project_id
  role    = "roles/compute.viewer"
  member  = "serviceAccount:${var.pbs_service_account_email}"
}

resource "google_project_iam_member" "pbs_tag_iam_viewer" {
  project = var.project_id
  role    = "roles/resourcemanager.tagViewer"
  member  = "serviceAccount:${var.pbs_service_account_email}"
}

data "google_iam_policy" "cloud_run_no_auth" {
  binding {
    role = "roles/run.invoker"
    members = [
      "allUsers",
    ]
  }
}

resource "google_cloud_run_service_iam_policy" "no_auth" {
  location    = google_cloud_run_v2_service.pbs_instance.location
  project     = google_cloud_run_v2_service.pbs_instance.project
  service     = google_cloud_run_v2_service.pbs_instance.name
  policy_data = data.google_iam_policy.cloud_run_no_auth.policy_data
}

resource "google_cloud_run_v2_service" "pbs_instance" {
  project  = var.project_id
  name     = "${var.environment}-${var.region}-pbs-cloud-run"
  location = var.region
  ingress  = "INGRESS_TRAFFIC_INTERNAL_LOAD_BALANCER"
  template {
    service_account = var.pbs_service_account_email
    containers {
      image = local.pbs_image_cr
      ports {
        name           = "h2c"
        container_port = var.main_port
      }
      dynamic "env" {
        for_each = local.pbs_cloud_run_environment_variables_merged
        content {
          name  = env.key
          value = env.value
        }
      }
      resources {
        limits = {
          cpu    = 8
          memory = "8Gi"
        }
        startup_cpu_boost = true
        cpu_idle          = false
      }
    }
    scaling {
      min_instance_count = var.pbs_cloud_run_min_instances
      max_instance_count = var.pbs_cloud_run_max_instances
    }
    max_instance_request_concurrency = var.pbs_cloud_run_max_concurrency
    labels = {
      # Create a new revision if pbs_cloud_run_revision_force_replace is true. This
      # is done by applying a unique timestamp label on each deployment.
      force_new_revision_timestamp = var.pbs_cloud_run_revision_force_replace ? formatdate("YYYY-MM-DD_hh_mm_ss", timestamp()) : null,
    }
  }
  traffic {
    type    = "TRAFFIC_TARGET_ALLOCATION_TYPE_LATEST"
    percent = 100
  }
}
