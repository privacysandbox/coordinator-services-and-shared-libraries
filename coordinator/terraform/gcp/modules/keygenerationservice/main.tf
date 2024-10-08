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
      version = ">= 5.21"
    }
  }
}

locals {
  key_generation_service_account_email = google_service_account.key_generation_service_account.email
}
module "version" {
  source = "../version"
}

resource "google_service_account" "key_generation_service_account" {
  # Service account id has a 30 character limit
  account_id   = "${var.environment}-keygenuser"
  display_name = "Key Generation Service Account"
}

# Cloud KMS encryption ring and key encryption key (KEK)
resource "google_kms_key_ring" "key_encryption_ring" {
  name     = "${var.environment}_key_encryption_ring"
  location = "us"
}

resource "google_kms_crypto_key" "key_encryption_key" {
  name     = "${var.environment}_key_encryption_key"
  key_ring = google_kms_key_ring.key_encryption_ring.id

  lifecycle {
    prevent_destroy = true
  }
}

# Allow key generation service account to encrypt with KEK
resource "google_kms_key_ring_iam_member" "key_encryption_ring_iam" {
  key_ring_id = google_kms_key_ring.key_encryption_ring.id
  role        = "roles/cloudkms.cryptoKeyEncrypter"
  member      = "serviceAccount:${local.key_generation_service_account_email}"
}

# KeyDB read/write permissions
resource "google_spanner_database_iam_member" "key_generation_keydb_iam" {
  instance = var.spanner_instance_name
  database = var.spanner_database_name
  role     = "roles/spanner.databaseUser"
  member   = "serviceAccount:${local.key_generation_service_account_email}"
}

resource "google_project_iam_member" "key_generation_secretmanager_iam" {
  project = var.project_id
  role    = "roles/secretmanager.secretAccessor"
  member  = "serviceAccount:${local.key_generation_service_account_email}"
}

# Logging permissions for key generation service account
resource "google_project_iam_member" "key_generation_log_writer_iam" {
  project = var.project_id
  role    = "roles/logging.logWriter"
  member  = "serviceAccount:${local.key_generation_service_account_email}"
}

resource "google_project_iam_member" "key_generation_metric_writer_iam" {
  project = var.project_id
  role    = "roles/monitoring.metricWriter"
  member  = "serviceAccount:${local.key_generation_service_account_email}"
}

resource "google_project_iam_member" "key_generation_workload_user_iam" {
  project = var.project_id
  role    = "roles/confidentialcomputing.workloadUser"
  member  = "serviceAccount:${local.key_generation_service_account_email}"
}

resource "google_project_iam_member" "key_generation_artifact_reader_iam" {
  project = var.project_id
  role    = "roles/artifactregistry.reader"
  member  = "serviceAccount:${local.key_generation_service_account_email}"
}

resource "null_resource" "key_generation_instance_replace_trigger" {
  triggers = {
    replace = var.key_gen_instance_force_replace ? timestamp() : ""
  }
}

resource "google_compute_region_autoscaler" "keygen_autoscaler" {
  project = var.project_id
  region  = var.region

  name   = "keygen-auto-scaler"
  target = google_compute_region_instance_group_manager.keygen_instance_group.id

  autoscaling_policy {
    max_replicas = 1
    min_replicas = 0

    cooldown_period = 300

    metric {
      name                       = "pubsub.googleapis.com/subscription/num_undelivered_messages"
      filter                     = "resource.type = pubsub_subscription AND resource.label.subscription_id = ${google_pubsub_subscription.key_generation_pubsub_subscription.name}"
      single_instance_assignment = 100
    }
  }

  lifecycle {
    replace_triggered_by = [
      # Auto-scaler configuration must be replaced with the instance group.
      google_compute_region_instance_group_manager.keygen_instance_group.id,
    ]
  }
}

resource "google_compute_region_instance_group_manager" "keygen_instance_group" {
  name               = "${var.environment}-keygen-vm-mig"
  description        = "The managed instance group for Key generation instance."
  base_instance_name = "${var.environment}-keygen"
  region             = var.region

  version {
    instance_template = google_compute_instance_template.key_generation_vm.id
  }

  update_policy {
    minimal_action = "RESTART"
    type           = "OPPORTUNISTIC"
    # Must be at least the number of zones for this group. The default is 3.
    max_unavailable_fixed = 3
  }

  lifecycle {
    replace_triggered_by = [
      # Replacing the instance group manager will terminate any running instances.
      null_resource.key_generation_instance_replace_trigger,
    ]
  }
}

resource "google_compute_instance_template" "key_generation_vm" {
  project      = var.project_id
  name_prefix  = format("%s-keygen", var.environment)
  machine_type = "n2d-standard-2"

  tags = compact([ # compact filters out empty strings.
    "environment",
    var.environment,
    var.egress_internet_tag
  ])

  disk {
    boot         = true
    device_name  = "${var.environment}-keygen"
    source_image = var.instance_disk_image
  }

  network_interface {
    network = var.network
  }

  metadata = {
    coordinator-version              = module.version.version
    google-logging-enabled           = var.key_generation_logging_enabled
    google-monitoring-enabled        = var.key_generation_monitoring_enabled
    scp-environment                  = var.environment
    tee-image-reference              = var.key_generation_image
    tee-restart-policy               = var.key_generation_tee_restart_policy
    tee-container-log-redirect       = var.key_generation_logging_enabled
    tee-impersonate-service-accounts = var.key_generation_tee_allowed_sa
  }

  labels = {
    # Replace slashes with dashes due to format restrictions
    container-vm = replace(var.instance_disk_image, "/", "-"),
    environment  = var.environment
  }

  service_account {
    email  = local.key_generation_service_account_email
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

  lifecycle {
    # Cannot be destroyed while attached to a managed instance group.
    create_before_destroy = true
  }
}
