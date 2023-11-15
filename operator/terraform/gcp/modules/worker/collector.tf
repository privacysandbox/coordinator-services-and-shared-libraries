/**
 * Copyright 2023 Google LLC
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

#################################################################
#
#                         Collector
#
# The collector receives and forwards gRPC OpenTelemetry traffic.
#################################################################

# Internal Load Balancer

resource "google_compute_region_backend_service" "collector" {
  name                  = "${var.environment}-${var.collector_service_name}-backend-service"
  protocol              = "TCP"
  load_balancing_scheme = "INTERNAL"
  health_checks         = [google_compute_health_check.collector.id]
  backend {
    group          = google_compute_region_instance_group_manager.collector.instance_group
    balancing_mode = "CONNECTION"
  }
}

resource "google_compute_forwarding_rule" "collector" {
  name                  = "${var.environment}-${var.collector_service_name}-forwarding-rule"
  backend_service       = google_compute_region_backend_service.collector.id
  ip_protocol           = "TCP"
  load_balancing_scheme = "INTERNAL"
  all_ports             = true
  allow_global_access   = true
  network               = var.network
}

# Collector

resource "google_compute_instance_template" "collector" {
  name        = "${var.environment}-${var.collector_service_name}"
  description = "This template is used to create an opentelemetry collector."
  project     = var.project_id
  tags        = ["allow-otlp", "allow-hc", "allow-all-egress", var.egress_internet_tag]
  disk {
    auto_delete  = true
    boot         = true
    device_name  = "persistent-disk-0"
    disk_type    = "pd-standard"
    mode         = "READ_WRITE"
    source_image = "debian-cloud/debian-11"
    type         = "PERSISTENT"
  }
  labels = {
    environment = var.environment
    service     = var.collector_service_name
  }
  network_interface {
    network = var.network
  }
  machine_type = var.collector_machine_type
  service_account {
    email  = local.worker_service_account_email
    scopes = ["https://www.googleapis.com/auth/cloud-platform"]
  }
  metadata = {
    startup-script = templatefile("${var.collector_startup_script}", {
      collector_port = var.collector_service_port,
    })
  }
  lifecycle {
    create_before_destroy = true
  }
}

resource "google_compute_region_instance_group_manager" "collector" {
  name = "${var.environment}-${var.collector_service_name}-manager"
  version {
    instance_template = google_compute_instance_template.collector.self_link
    name              = "primary"
  }
  named_port {
    name = "otlp"
    port = var.collector_service_port
  }
  base_instance_name = "${var.collector_service_name}-${var.environment}"
  auto_healing_policies {
    health_check      = google_compute_health_check.collector.id
    initial_delay_sec = var.vm_startup_delay_seconds
  }
}

resource "google_compute_region_autoscaler" "collector" {
  name   = "${var.environment}-${var.collector_service_name}-as"
  target = google_compute_region_instance_group_manager.collector.self_link
  autoscaling_policy {
    max_replicas    = var.max_collectors_per_region
    min_replicas    = 1
    cooldown_period = var.vm_startup_delay_seconds
    cpu_utilization {
      target = var.target_cpu_utilization
    }
  }
}

resource "google_compute_health_check" "collector" {
  name = "${var.environment}-${var.collector_service_name}-auto-heal-hc"
  tcp_health_check {
    port_name = "otlp"
    port      = var.collector_service_port
  }
  timeout_sec         = 3
  check_interval_sec  = 3
  healthy_threshold   = 2
  unhealthy_threshold = 4
  log_config {
    enable = true
  }
}

resource "google_compute_firewall" "fw_allow_otlp" {
  name          = "${var.environment}-${var.collector_service_name}-fw-allow-otlp"
  direction     = "INGRESS"
  network       = var.network
  source_ranges = ["10.128.0.0/16"]
  allow {
    protocol = "tcp"
    ports    = [var.collector_service_port]
  }
  target_tags = ["allow-otlp"]
}

resource "google_compute_firewall" "fw_allow_hc" {
  name          = "${var.environment}-${var.collector_service_name}-fw-allow-hc"
  provider      = google
  direction     = "INGRESS"
  network       = var.network
  source_ranges = ["130.211.0.0/22", "35.191.0.0/16", "35.235.240.0/20"]
  allow {
    protocol = "tcp"
  }
  target_tags = ["allow-hc"]
}

resource "google_compute_firewall" "fw_allow_all_egress" {
  name      = "${var.environment}-${var.collector_service_name}-fw-allow-all-egress"
  direction = "EGRESS"
  network   = var.network
  allow {
    protocol = "tcp"
  }
  target_tags = ["allow-all-egress"]
}
