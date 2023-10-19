/**
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
locals {
  worker_service_account_email = var.user_provided_worker_sa_email == "" ? google_service_account.worker_service_account[0].email : var.user_provided_worker_sa_email
  disk_image_family            = split("/", var.instance_disk_image)[1]
  disk_image_project           = split("/", var.instance_disk_image)[0]
}

resource "google_service_account" "worker_service_account" {
  count = var.user_provided_worker_sa_email == "" ? 1 : 0
  # Service account id has a 30 character limit
  account_id   = "${var.environment}-worker"
  display_name = "Worker Service Account"
}

data "google_compute_image" "tee_image" {
  family  = local.disk_image_family
  project = local.disk_image_project
}

resource "null_resource" "worker_instance_replace_trigger" {
  triggers = {
    replace = var.worker_instance_force_replace ? timestamp() : ""
  }
}

resource "google_compute_instance_template" "worker_instance_template" {

  name_prefix  = "${var.environment}-worker-template"
  machine_type = var.instance_type

  disk {
    boot         = true
    device_name  = "${var.environment}-worker"
    source_image = data.google_compute_image.tee_image.self_link
  }

  # TODO: Add custom VPC configurations
  network_interface {
    network = var.network
  }

  metadata = {
    google-logging-enabled           = var.worker_logging_enabled,
    google-monitoring-enabled        = var.worker_monitoring_enabled,
    scp-environment                  = var.environment,
    tee-image-reference              = var.worker_image,
    tee-restart-policy               = var.worker_restart_policy,
    tee-impersonate-service-accounts = join(",", [var.coordinator_a_impersonate_service_account, var.coordinator_b_impersonate_service_account])
    tee-container-log-redirect       = var.worker_logging_enabled
  }

  service_account {
    # Google recommends custom service accounts that have cloud-platform scope and permissions granted via IAM Roles.
    email  = local.worker_service_account_email
    scopes = ["cloud-platform"]
  }

  scheduling {
    # Confidential compute requires on_host_maintenance to be TERMINATE
    on_host_maintenance = "TERMINATE"
  }

  confidential_instance_config {
    enable_confidential_compute = true
  }

  shielded_instance_config {
    enable_secure_boot = true
  }

  tags = compact([ # compact filters out empty strings.
    "environment",
    var.environment,
    var.egress_internet_tag
  ])

  # Create before destroy since template is being used by worker instance group
  lifecycle {
    create_before_destroy = true
    replace_triggered_by = [
      null_resource.worker_instance_replace_trigger
    ]
  }
}

# JobMetadata read/write permissions
resource "google_spanner_database_iam_member" "worker_jobmetadatadb_iam" {
  count    = var.user_provided_worker_sa_email == "" ? 1 : 0
  instance = var.metadatadb_instance_name
  database = var.metadatadb_name
  role     = "roles/spanner.databaseUser"
  member   = "serviceAccount:${local.worker_service_account_email}"
}

# JobQueue read/write permissions
resource "google_pubsub_subscription_iam_member" "worker_jobqueue_iam" {
  count        = var.user_provided_worker_sa_email == "" ? 1 : 0
  role         = "roles/pubsub.subscriber"
  member       = "serviceAccount:${local.worker_service_account_email}"
  subscription = var.job_queue_sub
}

resource "google_project_iam_member" "worker_storage_iam" {
  count   = var.user_provided_worker_sa_email == "" ? 1 : 0
  role    = "roles/storage.admin"
  member  = "serviceAccount:${local.worker_service_account_email}"
  project = var.project_id
}

resource "google_project_iam_member" "worker_secretmanager_iam" {
  count   = var.user_provided_worker_sa_email == "" ? 1 : 0
  role    = "roles/secretmanager.secretAccessor"
  member  = "serviceAccount:${local.worker_service_account_email}"
  project = var.project_id
}

resource "google_project_iam_member" "worker_logging_iam" {
  count   = var.user_provided_worker_sa_email == "" ? 1 : 0
  role    = "roles/logging.logWriter"
  member  = "serviceAccount:${local.worker_service_account_email}"
  project = var.project_id
}

resource "google_project_iam_member" "worker_traces_iam" {
  role    = "roles/cloudtrace.admin"
  member  = "serviceAccount:${local.worker_service_account_email}"
  project = var.project_id
}

resource "google_project_iam_member" "worker_monitoring_iam" {
  count   = var.user_provided_worker_sa_email == "" ? 1 : 0
  role    = "roles/monitoring.editor"
  member  = "serviceAccount:${local.worker_service_account_email}"
  project = var.project_id
}

resource "google_project_iam_member" "worker_instance_group_iam" {
  count   = var.user_provided_worker_sa_email == "" ? 1 : 0
  role    = "roles/compute.networkAdmin"
  member  = "serviceAccount:${local.worker_service_account_email}"
  project = var.project_id
}

resource "google_project_iam_member" "worker_workload_user_iam" {
  count   = var.user_provided_worker_sa_email == "" ? 1 : 0
  role    = "roles/confidentialcomputing.workloadUser"
  member  = "serviceAccount:${local.worker_service_account_email}"
  project = var.project_id
}

resource "google_project_iam_member" "worker_pubsub_publisher_iam" {
  count   = var.user_provided_worker_sa_email == "" ? 1 : 0
  role    = "roles/pubsub.publisher"
  member  = "serviceAccount:${local.worker_service_account_email}"
  project = var.project_id
}
