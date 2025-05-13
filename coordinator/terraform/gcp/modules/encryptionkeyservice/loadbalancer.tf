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

# Network Endpoint Group to route to Cloud Run
resource "google_compute_region_network_endpoint_group" "encryption_key_service_cloud_run" {
  project               = var.project_id
  name                  = "${var.environment}-${var.region}-encryption-key-service-cloud-run"
  network_endpoint_type = "SERVERLESS"
  region                = var.region
  cloud_run {
    service = google_cloud_run_v2_service.private_key_service.name
  }
}

module "encryption_key_service_security_policy" {
  source = "../shared/cloudarmor_security_policy"

  project_id                  = var.project_id
  security_policy_name        = "${var.environment}-encryption-key-service-security-policy"
  security_policy_description = "Security policy for Encryption Key Service LB"
  use_adaptive_protection     = var.use_adaptive_protection
  security_policy_rules       = var.encryption_key_security_policy_rules
}
moved {
  from = google_compute_security_policy.encryption_key_service_security_policy
  to   = module.encryption_key_service_security_policy.google_compute_security_policy.security_policy
}

# Monitoring alerts and notifications for Cloud Armor security policies.
module "encryption_key_service_cloudarmor_alarms" {
  count  = var.enable_cloud_armor_alerts ? 1 : 0
  source = "../shared/cloudarmor_alarms"

  project_id              = var.project_id
  environment             = var.environment
  notifications_enabled   = var.enable_cloud_armor_notifications
  notification_channel_id = var.cloud_armor_notification_channel_id
  security_policy_name    = module.encryption_key_service_security_policy.security_policy.name
  service_prefix          = "${var.environment} Encryption Key Service"
}

# Backend service that groups network endpoint groups to Cloud Run for Load
# Balancer to use.
resource "google_compute_backend_service" "encryption_key_service_cloud_run" {
  project     = var.project_id
  name        = "${var.environment}-encryption-key-service-cloud-run"
  description = "Load balancer backend service for Encryption Key services."

  enable_cdn            = false
  load_balancing_scheme = "EXTERNAL_MANAGED"

  backend {
    description = var.environment
    group       = google_compute_region_network_endpoint_group.encryption_key_service_cloud_run.id
  }

  log_config {
    enable = true
  }

  security_policy = var.enable_security_policy ? module.encryption_key_service_security_policy.security_policy.id : null
}

# URL Map creates Load balancer to Cloud Run
resource "google_compute_url_map" "encryption_key_service_cloud_run" {
  project         = var.project_id
  name            = "${var.environment}-encryption-key-service-cloud-run"
  default_service = google_compute_backend_service.encryption_key_service_cloud_run.id
}

# Proxy to loadbalancer for Cloud Run. HTTP without custom domain
resource "google_compute_target_http_proxy" "encryption_key_service_cloud_run" {
  count = !var.enable_domain_management ? 1 : 0

  project = var.project_id
  name    = "${var.environment}-encryption-key-service-cloud-run"
  url_map = google_compute_url_map.encryption_key_service_cloud_run.id
}

# Proxy to loadbalancer for Cloud Run. HTTPS with custom domain
resource "google_compute_target_https_proxy" "encryption_key_service_cloud_run" {
  count = var.enable_domain_management ? 1 : 0

  project = var.project_id
  name    = "${var.environment}-encryption-key-service-cloud-run"
  url_map = google_compute_url_map.encryption_key_service_cloud_run.id

  ssl_certificates = [
    google_compute_managed_ssl_certificate.encryption_key_service_loadbalancer[0].id,
  ]
}

# Reserve IP address.
resource "google_compute_global_address" "encryption_key_service_cloud_run" {
  project = var.project_id
  name    = "${var.environment}-encryption-key-service-cloud-run"
}

# Map IP address and loadbalancer proxy to Cloud Run
resource "google_compute_global_forwarding_rule" "encryption_key_service_cloud_run" {
  project               = var.project_id
  name                  = "${var.environment}-encryption-key-service-cloud-run"
  ip_address            = google_compute_global_address.encryption_key_service_cloud_run.address
  port_range            = var.enable_domain_management ? "443" : "80"
  load_balancing_scheme = "EXTERNAL_MANAGED"

  target = (
    var.enable_domain_management ?
    google_compute_target_https_proxy.encryption_key_service_cloud_run[0].id :
    google_compute_target_http_proxy.encryption_key_service_cloud_run[0].id
  )
}

# Creates SSL cert for given domain. Terraform does not wait for SSL cert to be provisioned before the `apply` operation
# succeeds. As long as the hosted zone exists, it can take up to 20 mins for the cert to be provisioned.
# See console for status: https://console.cloud.google.com/loadbalancing/advanced/sslCertificates/list
# Note: even if status of cert becomes 'Active', it can still take around 10 mins for requests to the domain to work.
resource "google_compute_managed_ssl_certificate" "encryption_key_service_loadbalancer" {
  count   = var.enable_domain_management ? 1 : 0
  project = var.project_id
  name    = "${var.environment}-encryption-key-cert"

  managed {
    domains = [var.encryption_key_domain]
  }
}
