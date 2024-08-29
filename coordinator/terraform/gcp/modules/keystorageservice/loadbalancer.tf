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

resource "google_compute_backend_service" "key_storage" {
  project = var.project_id
  name    = "${local.name}-backend-default"

  backend {
    group = google_compute_region_network_endpoint_group.key_storage_endpoint_group.id
  }
}
moved {
  from = module.lb-http_serverless_negs.google_compute_backend_service.default["default"]
  to   = google_compute_backend_service.key_storage
}

resource "google_compute_url_map" "key_storage" {
  project         = var.project_id
  name            = "${local.name}-url-map"
  default_service = google_compute_backend_service.key_storage.id
}
moved {
  from = module.lb-http_serverless_negs.google_compute_url_map.default[0]
  to   = google_compute_url_map.key_storage
}

resource "google_compute_global_address" "key_storage" {
  project = var.project_id
  name    = "${local.name}-address"
}
moved {
  from = module.lb-http_serverless_negs.google_compute_global_address.default[0]
  to   = google_compute_global_address.key_storage
}

resource "google_compute_target_http_proxy" "key_storage" {
  count   = var.enable_domain_management ? 0 : 1
  project = var.project_id
  name    = "${local.name}-http-proxy"
  url_map = google_compute_url_map.key_storage.id
}
moved {
  from = module.lb-http_serverless_negs.google_compute_target_http_proxy.default
  to   = google_compute_target_http_proxy.key_storage
}

resource "google_compute_global_forwarding_rule" "http" {
  count      = var.enable_domain_management ? 0 : 1
  project    = var.project_id
  name       = local.name
  target     = google_compute_target_http_proxy.key_storage[0].id
  ip_address = google_compute_global_address.key_storage.address
  port_range = "80"
}
moved {
  from = module.lb-http_serverless_negs.google_compute_global_forwarding_rule.http
  to   = google_compute_global_forwarding_rule.http
}

resource "google_compute_target_https_proxy" "key_storage" {
  count   = var.enable_domain_management ? 1 : 0
  project = var.project_id
  name    = "${local.name}-https-proxy"
  url_map = google_compute_url_map.key_storage.id

  ssl_certificates = [google_compute_managed_ssl_certificate.key_storage[0].id]
}
moved {
  from = module.lb-http_serverless_negs.google_compute_target_https_proxy.default
  to   = google_compute_target_https_proxy.key_storage
}

resource "google_compute_global_forwarding_rule" "https" {
  count      = var.enable_domain_management ? 1 : 0
  project    = var.project_id
  name       = "${local.name}-https"
  target     = google_compute_target_https_proxy.key_storage[0].id
  ip_address = google_compute_global_address.key_storage.address
  port_range = "443"
}
moved {
  from = module.lb-http_serverless_negs.google_compute_global_forwarding_rule.https
  to   = google_compute_global_forwarding_rule.https
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
moved {
  from = module.lb-http_serverless_negs.google_compute_managed_ssl_certificate.default
  to   = google_compute_managed_ssl_certificate.key_storage
}

resource "google_compute_region_network_endpoint_group" "key_storage_endpoint_group" {
  name                  = "${var.environment}-keystoragegroup"
  network_endpoint_type = "SERVERLESS"
  region                = var.region
  cloud_run {
    service = google_cloudfunctions2_function.key_storage_cloudfunction.name
  }
}
