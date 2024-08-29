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

data "google_compute_zones" "available_zones" {}

locals {
  target_instance_count = 3
  zone_usage_count      = length(data.google_compute_zones.available_zones)

  docker_cert_volume_name    = "pbscertsvolume"
  host_certificate_path      = "/tmp/self-signed-certs"
  container_certificate_path = "/opt/google/ssl/self-signed-certs"

  pbs_log_volume_name    = "pbslogs"
  pbs_container_log_path = "/var/log/"
  pbs_host_log_path      = "/var/log/pbs/"

  startup_script = templatefile("${path.module}/files/instance_startup.sh", { host_certificate_path = local.host_certificate_path })

  pbs_auth_endpoint = var.enable_domain_management ? "${var.pbs_domain}/v1/auth" : var.pbs_auth_audience_url
  pbs_application_environment_variables = concat(var.pbs_application_environment_variables, [
    {
      name  = "google_scp_gcp_project_id"
      value = var.project
    },
    {
      name  = "google_scp_core_cloud_region"
      value = var.region
    },
    # This is required for Gcp Metric as the default aggregated metric interval
    # value is 1000, which exceeds Gcp custom metric quota limit (one data
    # point per 5 seconds).
    {
      name  = "google_scp_aggregated_metric_interval_ms"
      value = "5000"
    },
    {
      name  = "google_scp_pbs_journal_service_bucket_name"
      value = var.pbs_cloud_storage_journal_bucket_name
    },
    {
      name  = "google_scp_spanner_instance_name"
      value = var.pbs_spanner_instance_name
    },
    {
      name  = "google_scp_spanner_database_name"
      value = var.pbs_spanner_database_name
    },
    {
      name  = "google_scp_pbs_budget_key_table_name"
      value = var.pbs_spanner_budget_key_table_name
    },
    {
      name  = "google_scp_pbs_partition_lock_table_name"
      value = var.pbs_spanner_partition_lock_table_name
    },
    {
      name  = "google_scp_pbs_auth_endpoint"
      value = local.pbs_auth_endpoint
    },
    {
      name  = "google_scp_pbs_host_port"
      value = var.main_port
    },
    {
      name  = "google_scp_pbs_external_exposed_host_port"
      value = var.main_port
    },
    {
      name  = "google_scp_pbs_health_port"
      value = var.health_check_port
    },
    {
      name  = "google_scp_pbs_http2_server_private_key_file_path"
      value = "${local.container_certificate_path}/privatekey.pem"
    },
    {
      name  = "google_scp_pbs_http2_server_certificate_file_path"
      value = "${local.container_certificate_path}/public.crt"
    },
    {
      name  = "google_scp_pbs_http2_server_use_tls"
      value = "true"
    },
    {
      name  = "google_scp_pbs_partition_lease_duration_in_seconds"
      value = "5"
    },
    {
      name  = "google_scp_pbs_async_executor_queue_size"
      value = "100000000"
    },
    {
      name  = "google_scp_pbs_io_async_executor_queue_size"
      value = "100000000"
    },
    {
      name  = "google_scp_pbs_transaction_manager_capacity"
      value = "100000000"
    },
    {
      name  = "google_scp_pbs_journal_service_partition_name"
      value = "00000000-0000-0000-0000-000000000000"
    },
    {
      name  = "google_scp_pbs_host_address"
      value = "0.0.0.0"
    },
    {
      name  = "google_scp_pbs_remote_claimed_identity"
      value = "remote-coordinator.com"
    },
    {
      name  = "google_scp_pbs_metrics_namespace"
      value = "${var.environment}-google-scp-pbs"
    },
    {
      name  = "google_scp_pbs_metrics_batch_push_enabled"
      value = "true"
    },
    {
      name  = "google_scp_pbs_metrics_batch_time_duration_ms"
      value = "3000"
    },
    {
      name  = "google_scp_pbs_multi_instance_mode_disabled"
      value = local.target_instance_count > 1 || var.pbs_autoscaling_policy != null ? "false" : "true"
    },
    {
      name  = "google_scp_transaction_manager_skip_duplicate_transaction_in_recovery"
      value = "true"
    },
    {
      name  = "google_scp_pbs_relaxed_consistency_enabled"
      value = "true"
    },
  ])
}

resource "google_artifact_registry_repository_iam_member" "pbs_artifact_registry_iam_read" {
  project    = var.project
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
  instance = var.pbs_spanner_instance_name
  database = var.pbs_spanner_database_name
  role     = "roles/spanner.databaseUser"
  member   = "serviceAccount:${var.pbs_service_account_email}"
}

resource "google_project_iam_member" "pbs_logging_iam_writer" {
  project = var.project
  role    = "roles/logging.logWriter"
  member  = "serviceAccount:${var.pbs_service_account_email}"
}

resource "google_project_iam_member" "pbs_metric_iam_writer" {
  project = var.project
  role    = "roles/monitoring.metricWriter"
  member  = "serviceAccount:${var.pbs_service_account_email}"
}

resource "google_project_iam_member" "pbs_compute_iam_viewer" {
  project = var.project
  role    = "roles/compute.viewer"
  member  = "serviceAccount:${var.pbs_service_account_email}"
}

resource "google_project_iam_member" "pbs_tag_iam_viewer" {
  project = var.project
  role    = "roles/resourcemanager.tagViewer"
  member  = "serviceAccount:${var.pbs_service_account_email}"
}

# The following options cannot be used together.
# `name` can be specified for a specific cos stable OS version
# `family` can be specified for the latest cos stable OS.
# See https://registry.terraform.io/providers/hashicorp/google/latest/docs/data-sources/compute_image.
## IMPORTANT: Here we have pinned `cos-105-17412-370-67` which has LTS due to the upcoming change from
## fluentd to fluent-bit. Preliminary testing shows enabling fluent-bit will break PBS logs. Migration
## must occur before the deprecation of cos-105-LTS (March 2025).
data "google_compute_image" "pbs_container_vm_image" {
  name    = "cos-105-17412-370-67"
  project = "cos-cloud"
}

module "pbs_container" {
  source  = "terraform-google-modules/container-vm/google"
  version = "~> 2.0"

  container = {
    image = "${var.region}-docker.pkg.dev/${var.project}/${var.pbs_artifact_registry_repository_name}/pbs-image:${var.pbs_image_tag}"
    securityContext = {
      privileged : true
    }
    tty : false

    env = local.pbs_application_environment_variables

    volumeMounts = [
      # Mount the self-signed cert directory from the host machine on the container
      # so that it can be accessed by the HTTP2 server.
      {
        name      = "${local.docker_cert_volume_name}"
        mountPath = "${local.container_certificate_path}"
        readOnly  = true
      },
      {
        name      = "${local.pbs_log_volume_name}"
        mountPath = "${local.pbs_container_log_path}"
        readOnly  = false
      },
    ],
  }

  volumes = [
    {
      name = local.docker_cert_volume_name
      hostPath = {
        path = local.host_certificate_path
      }
    },
    {
      name = local.pbs_log_volume_name
      hostPath = {
        path = local.pbs_host_log_path
      }
    },
  ]

  restart_policy = "Always"
}

resource "google_compute_instance_template" "pbs_instance_template" {
  # Cannot be longer than 37 characters
  name_prefix          = "${var.environment}-pbs-inst-template"
  description          = "The PBS template for creating instances for ${var.environment}."
  instance_description = "A PBS instance for ${var.environment}"
  machine_type         = var.machine_type

  disk {
    device_name  = "${var.environment}-pbs"
    source_image = data.google_compute_image.pbs_container_vm_image.self_link
    boot         = true
    auto_delete  = true
    disk_size_gb = var.root_volume_size_gb
    disk_type    = "pd-ssd"
  }

  network_interface {
    # If a VPC ID is provided, use that, otherwise use the default network.
    network    = var.vpc_network_id != "" ? var.vpc_network_id : "default"
    subnetwork = var.vpc_subnet_id

    # There is no specific flag to enable/disable auto-assigment of a public IP
    # address. However, this is controlled via the access_config block.
    # Excluding this block will result in a public IP address not being assigned.
    # https://registry.terraform.io/providers/hashicorp/google/latest/docs/resources/compute_instance_template#access_config
    dynamic "access_config" {
      for_each = var.enable_public_ip_address ? [1] : []
      content {
        network_tier = "PREMIUM"
      }
    }
  }

  metadata = {
    gce-container-declaration = module.pbs_container.metadata_value
    google-logging-enabled    = var.pbs_cloud_logging_enabled
    google-monitoring-enabled = var.pbs_cloud_monitoring_enabled
    startup-script            = local.startup_script
    enable-oslogin            = true
  }

  service_account {
    # Google recommends custom service accounts that have cloud-platform scope and permissions granted via IAM Roles.
    email  = var.pbs_service_account_email
    scopes = ["cloud-platform"]
  }

  scheduling {
    automatic_restart   = true
    on_host_maintenance = "MIGRATE"
  }

  shielded_instance_config {
    enable_secure_boot          = false
    enable_vtpm                 = true
    enable_integrity_monitoring = true
  }

  tags = concat([var.environment, var.network_target_tag], var.pbs_custom_vm_tags)

  # Create before destroy since template is being used by the PBS instance group
  lifecycle {
    create_before_destroy = true
  }
}

resource "google_compute_health_check" "pbs_health_check" {
  count   = var.enable_health_check ? 1 : 0
  name    = "${var.environment}-pbs-mig-hc"
  project = var.project

  check_interval_sec  = 10
  timeout_sec         = 10
  unhealthy_threshold = 5

  http2_health_check {
    request_path       = "/health"
    port               = var.health_check_port
    port_specification = "USE_FIXED_PORT"
    proxy_header       = "NONE"
  }
}

resource "google_compute_region_instance_group_manager" "pbs_instance_group" {
  name               = "${var.environment}-pbs-mig"
  description        = "The PBS managed instance group for ${var.environment}."
  base_instance_name = "${var.environment}-pbs"
  region             = var.region
  # target_size is managed by autoscaling if pbs_autoscaling_policy is set.
  target_size = var.pbs_autoscaling_policy == null ? local.target_instance_count : null

  version {
    instance_template = google_compute_instance_template.pbs_instance_template.id
  }

  update_policy {
    minimal_action               = "REPLACE"
    type                         = "PROACTIVE"
    instance_redistribution_type = "PROACTIVE"
    replacement_method           = "SUBSTITUTE"
    max_unavailable_fixed        = local.zone_usage_count
    max_surge_fixed              = local.zone_usage_count
  }

  dynamic "auto_healing_policies" {
    for_each = var.enable_health_check ? [1] : []
    content {
      health_check = google_compute_health_check.pbs_health_check[0].id
      // Wait 5 min when an instance first comes up before we start taking health checks into account.
      initial_delay_sec = 300
    }
  }

  dynamic "named_port" {
    for_each = var.named_ports
    content {
      name = named_port.value["name"]
      port = named_port.value["port"]
    }
  }

  lifecycle {
    create_before_destroy = true
  }
}

resource "google_compute_region_autoscaler" "pbs_instance_group" {
  count = var.pbs_autoscaling_policy != null ? 1 : 0

  name   = "${var.environment}-pbs-autoscaler"
  region = var.region
  target = google_compute_region_instance_group_manager.pbs_instance_group.id

  autoscaling_policy {
    max_replicas = var.pbs_autoscaling_policy.max_replicas
    min_replicas = var.pbs_autoscaling_policy.min_replicas

    cpu_utilization {
      target = var.pbs_autoscaling_policy.cpu_utilization_target
    }
  }
}
