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

# Network Endpoint Group to route to Cloud Functions in each region
resource "google_compute_region_network_endpoint_group" "get_public_key_network_endpoint_group" {
  for_each = !var.use_cloud_run ? google_cloudfunctions2_function.get_public_key_cloudfunction : {}

  project               = var.project_id
  name                  = "${var.environment}-${each.value.location}-get-public-key-endpoint-group"
  network_endpoint_type = "SERVERLESS"
  region                = each.value.location
  cloud_run {
    service = each.value.name
  }
}

# Network Endpoint Group to route to Cloud Run in each region
resource "google_compute_region_network_endpoint_group" "public_key_service_cloud_run" {
  for_each = var.use_cloud_run ? google_cloud_run_v2_service.public_key_service : {}

  project               = var.project_id
  name                  = "${var.environment}-${each.value.location}-public-key-service-cloud-run"
  network_endpoint_type = "SERVERLESS"
  region                = each.value.location
  cloud_run {
    service = each.value.name
  }
}

# Backend service that groups network endpoint groups to Cloud Functions for
# Load Balancer to use.
resource "google_compute_backend_service" "get_public_key_loadbalancer_backend" {
  count = !var.use_cloud_run ? 1 : 0

  name        = "${var.environment}-get-public-key-backend"
  project     = var.project_id
  description = "Backend service to point Public Key Cloud Functions."

  enable_cdn = var.enable_get_public_key_cdn

  cdn_policy {
    cache_mode  = var.enable_get_public_key_cdn ? "CACHE_ALL_STATIC" : null
    default_ttl = var.enable_get_public_key_cdn ? var.get_public_key_cloud_cdn_default_ttl_seconds : null
    client_ttl  = var.enable_get_public_key_cdn ? var.get_public_key_cloud_cdn_default_ttl_seconds : null
    max_ttl     = var.enable_get_public_key_cdn ? var.get_public_key_cloud_cdn_max_ttl_seconds : null

    cache_key_policy {
      include_host         = false
      include_protocol     = false
      include_query_string = false
    }
  }

  dynamic "backend" {
    for_each = google_compute_region_network_endpoint_group.get_public_key_network_endpoint_group
    content {
      description = var.environment
      group       = backend.value.id
    }
  }

  log_config {
    enable = var.public_key_load_balancer_logs_enabled
  }
}

# Backend service that groups network endpoint groups to Cloud Run for Load
# Balancer to use.
resource "google_compute_backend_service" "public_key_service_cloud_run" {
  count = var.use_cloud_run ? 1 : 0

  name        = "${var.environment}-public-key-service-cloud-run"
  project     = var.project_id
  description = "Load balancer backend service for Public Key services."

  enable_cdn = var.enable_get_public_key_cdn

  cdn_policy {
    cache_mode  = var.enable_get_public_key_cdn ? "CACHE_ALL_STATIC" : null
    default_ttl = var.enable_get_public_key_cdn ? var.get_public_key_cloud_cdn_default_ttl_seconds : null
    client_ttl  = var.enable_get_public_key_cdn ? var.get_public_key_cloud_cdn_default_ttl_seconds : null
    max_ttl     = var.enable_get_public_key_cdn ? var.get_public_key_cloud_cdn_max_ttl_seconds : null

    cache_key_policy {
      include_host         = false
      include_protocol     = false
      include_query_string = false
    }
  }

  dynamic "backend" {
    for_each = google_compute_region_network_endpoint_group.public_key_service_cloud_run
    content {
      description = var.environment
      group       = backend.value.id
    }
  }

  log_config {
    enable = var.public_key_load_balancer_logs_enabled
  }
}

# URL Map creates Load balancer to Cloud Functions
resource "google_compute_url_map" "get_public_key_loadbalancer" {
  count = !var.use_cloud_run ? 1 : 0

  project         = var.project_id
  name            = "${var.environment}-get-public-key-loadbalancer"
  default_service = google_compute_backend_service.get_public_key_loadbalancer_backend[0].id
}

# URL Map creates Load balancer to Cloud Run
resource "google_compute_url_map" "public_key_service_cloud_run" {
  count = var.use_cloud_run ? 1 : 0

  project         = var.project_id
  name            = "${var.environment}-public-key-service-cloud-run"
  default_service = google_compute_backend_service.public_key_service_cloud_run[0].id
}

# Proxy to loadbalancer for Cloud Functions. HTTP without custom domain
resource "google_compute_target_http_proxy" "get_public_key_loadbalancer_proxy" {
  count = !(var.use_cloud_run || var.enable_domain_management) ? 1 : 0

  project = var.project_id
  name    = "${var.environment}-get-public-key-proxy"
  url_map = google_compute_url_map.get_public_key_loadbalancer[0].id
}

# Proxy to loadbalancer for Cloud Functions. HTTPS with custom domain
resource "google_compute_target_https_proxy" "get_public_key_loadbalancer_proxy" {
  count = !var.use_cloud_run && var.enable_domain_management ? 1 : 0

  project = var.project_id
  name    = "${var.environment}-get-public-key-proxy"
  url_map = google_compute_url_map.get_public_key_loadbalancer[0].id

  ssl_certificates = [
    google_compute_managed_ssl_certificate.get_public_key_loadbalancer[0].id
  ]
}

# Proxy to loadbalancer for Cloud Run. HTTP without custom domain
resource "google_compute_target_http_proxy" "public_key_service_cloud_run" {
  count = var.use_cloud_run && !var.enable_domain_management ? 1 : 0

  project = var.project_id
  name    = "${var.environment}-public-key-service-cloud-run"
  url_map = google_compute_url_map.public_key_service_cloud_run[0].id
}

# Proxy to loadbalancer for Cloud Run. HTTPS with custom domain
resource "google_compute_target_https_proxy" "public_key_service_cloud_run" {
  count = var.use_cloud_run && var.enable_domain_management ? 1 : 0

  project = var.project_id
  name    = "${var.environment}-public-key-service-cloud-run"
  url_map = google_compute_url_map.public_key_service_cloud_run[0].id

  ssl_certificates = [
    google_compute_managed_ssl_certificate.get_public_key_loadbalancer[0].id
  ]
}

# Reserve IP address.
resource "google_compute_global_address" "get_public_key_ip_address" {
  project = var.project_id
  name    = "${var.environment}-get-public-key-ip-address"
}

# Map IP address and loadbalancer proxy to Cloud Functions
resource "google_compute_global_forwarding_rule" "get_public_key_loadbalancer_config" {
  count = !var.use_cloud_run ? 1 : 0

  project    = var.project_id
  name       = "${var.environment}-get-public-key-frontend-configuration"
  ip_address = google_compute_global_address.get_public_key_ip_address.address
  port_range = var.enable_domain_management ? "443" : "80"

  target = (
    var.enable_domain_management ?
    google_compute_target_https_proxy.get_public_key_loadbalancer_proxy[0].id :
    google_compute_target_http_proxy.get_public_key_loadbalancer_proxy[0].id
  )
}

# Map IP address and loadbalancer proxy to Cloud Run
resource "google_compute_global_forwarding_rule" "public_key_service_cloud_run" {
  count = var.use_cloud_run ? 1 : 0

  project    = var.project_id
  name       = "${var.environment}-public-key-service-cloud-run"
  ip_address = google_compute_global_address.get_public_key_ip_address.address
  port_range = var.enable_domain_management ? "443" : "80"

  target = (
    var.enable_domain_management ?
    google_compute_target_https_proxy.public_key_service_cloud_run[0].id :
    google_compute_target_http_proxy.public_key_service_cloud_run[0].id
  )
}

# Creates SSL cert for given domain. Terraform does not wait for SSL cert to be provisioned before the `apply` operation
# succeeds. As long as the hosted zone exists, it can take up to 20 mins for the cert to be provisioned.
# See console for status: https://console.cloud.google.com/loadbalancing/advanced/sslCertificates/list
# Note: even if status of cert becomes 'Active', it can still take around 10 mins for requests to the domain to work.
resource "google_compute_managed_ssl_certificate" "get_public_key_loadbalancer" {
  count   = var.enable_domain_management ? 1 : 0
  project = var.project_id
  name    = "${var.environment}-public-key-cert"

  managed {
    domains = concat([var.public_key_domain], var.public_key_service_alternate_domain_names)
  }
}
