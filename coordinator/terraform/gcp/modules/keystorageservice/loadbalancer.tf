# GCP Cloud Load Balancer Module
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

locals {
  name = "${var.environment}-${var.load_balancer_name}"
}

# Network Endpoint Group to route to Cloud Functions
resource "google_compute_region_network_endpoint_group" "key_storage_endpoint_group" {
  count = !var.use_cloud_run ? 1 : 0

  project               = var.project_id
  name                  = "${var.environment}-keystoragegroup"
  network_endpoint_type = "SERVERLESS"
  region                = var.region
  cloud_run {
    service = google_cloudfunctions2_function.key_storage_cloudfunction[0].name
  }
}

# Network Endpoint Group to route to Cloud Run
resource "google_compute_region_network_endpoint_group" "key_storage_service_cloud_run" {
  count = var.use_cloud_run ? 1 : 0

  project               = var.project_id
  name                  = "${var.environment}-key-storage-service-cloud-run"
  network_endpoint_type = "SERVERLESS"
  region                = var.region
  cloud_run {
    service = google_cloud_run_v2_service.key_storage_service[0].name
  }
}

# Backend service that groups network endpoint groups to Cloud Functions for
# Load Balancer to use.
resource "google_compute_backend_service" "key_storage" {
  count = !var.use_cloud_run ? 1 : 0

  project = var.project_id
  name    = "${local.name}-backend-default"

  backend {
    group = google_compute_region_network_endpoint_group.key_storage_endpoint_group[0].id
  }
}

# Backend service that groups network endpoint groups to Cloud Run for Load
# Balancer to use.
resource "google_compute_backend_service" "key_storage_service_cloud_run" {
  count = var.use_cloud_run ? 1 : 0

  project     = var.project_id
  name        = "${local.name}-key-storage-service-cloud-run"
  description = "Load balancer backend service for Key Storage services."

  backend {
    group = google_compute_region_network_endpoint_group.key_storage_service_cloud_run[0].id
  }
}

# URL Map creates Load balancer to Cloud Functions
resource "google_compute_url_map" "key_storage" {
  count = !var.use_cloud_run ? 1 : 0

  project         = var.project_id
  name            = "${local.name}-url-map"
  default_service = google_compute_backend_service.key_storage[0].id
}

# URL Map creates Load balancer to Cloud Run
resource "google_compute_url_map" "key_storage_service_cloud_run" {
  count = var.use_cloud_run ? 1 : 0

  project         = var.project_id
  name            = "${local.name}-key-storage-service-cloud-run"
  default_service = google_compute_backend_service.key_storage_service_cloud_run[0].id
}

# Proxy to loadbalancer for Cloud Functions. HTTP without custom domain
resource "google_compute_target_http_proxy" "key_storage" {
  count = !(var.use_cloud_run || var.enable_domain_management) ? 1 : 0

  project = var.project_id
  name    = "${local.name}-http-proxy"
  url_map = google_compute_url_map.key_storage[0].id
}

# Proxy to loadbalancer for Cloud Functions. HTTPS with custom domain
resource "google_compute_target_https_proxy" "key_storage" {
  count = !var.use_cloud_run && var.enable_domain_management ? 1 : 0

  project = var.project_id
  name    = "${local.name}-https-proxy"
  url_map = google_compute_url_map.key_storage[0].id

  ssl_certificates = [
    google_compute_managed_ssl_certificate.key_storage[0].id
  ]
}

# Proxy to loadbalancer for Cloud Run. HTTP without custom domain
resource "google_compute_target_http_proxy" "key_storage_service_cloud_run" {
  count = var.use_cloud_run && !var.enable_domain_management ? 1 : 0

  name    = "${local.name}-key-storage-service-cloud-run"
  url_map = google_compute_url_map.key_storage_service_cloud_run[0].id
}

# Proxy to loadbalancer for Cloud Run. HTTPS with custom domain
resource "google_compute_target_https_proxy" "key_storage_service_cloud_run" {
  count = var.use_cloud_run && var.enable_domain_management ? 1 : 0

  project = var.project_id
  name    = "${local.name}-key-storage-service-cloud-run"
  url_map = google_compute_url_map.key_storage_service_cloud_run[0].id

  ssl_certificates = [
    google_compute_managed_ssl_certificate.key_storage[0].id
  ]
}

resource "google_compute_global_address" "key_storage" {
  project = var.project_id
  name    = "${local.name}-address"
}

# Map IP address and loadbalancer HTTP proxy to Cloud Functions
resource "google_compute_global_forwarding_rule" "http" {
  count = (!var.use_cloud_run) && (!var.enable_domain_management) ? 1 : 0

  project    = var.project_id
  name       = local.name
  target     = google_compute_target_http_proxy.key_storage[0].id
  ip_address = google_compute_global_address.key_storage.address
  port_range = "80"
}

# Map IP address and loadbalancer HTTPS proxy to Cloud Functions
resource "google_compute_global_forwarding_rule" "https" {
  count = (!var.use_cloud_run) && var.enable_domain_management ? 1 : 0

  project    = var.project_id
  name       = "${local.name}-https"
  target     = google_compute_target_https_proxy.key_storage[0].id
  ip_address = google_compute_global_address.key_storage.address
  port_range = "443"
}

# Map IP address and loadbalancer proxy to Cloud Run
resource "google_compute_global_forwarding_rule" "key_storage_service_cloud_run" {
  count = var.use_cloud_run ? 1 : 0

  project    = var.project_id
  name       = "${local.name}-key-storage-service-cloud-run"
  ip_address = google_compute_global_address.key_storage.address
  port_range = var.enable_domain_management ? "443" : "80"

  target = (
    var.enable_domain_management ?
    google_compute_target_https_proxy.key_storage_service_cloud_run[0].id :
    google_compute_target_http_proxy.key_storage_service_cloud_run[0].id
  )
}

# Creates SSL cert for given domain. Terraform does not wait for SSL cert to be provisioned before the `apply` operation
# succeeds. As long as the hosted zone exists, it can take up to 20 mins for the cert to be provisioned.
# See console for status: https://console.cloud.google.com/loadbalancing/advanced/sslCertificates/list
# Note: even if status of cert becomes 'Active', it can still take around 10 mins for requests to the domain to work.
resource "google_compute_managed_ssl_certificate" "key_storage" {
  count   = var.enable_domain_management ? 1 : 0
  project = var.project_id
  name    = "${local.name}-cert"

  managed {
    domains = [var.key_storage_domain]
  }
}
