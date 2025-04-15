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

# Network Endpoint Group to route to Cloud Run in each region
resource "google_compute_region_network_endpoint_group" "public_key_service_cloud_run" {
  for_each = google_cloud_run_v2_service.public_key_service

  project               = var.project_id
  name                  = "${var.environment}-${each.value.location}-public-key-service-cloud-run"
  network_endpoint_type = "SERVERLESS"
  region                = each.value.location
  cloud_run {
    service = each.value.name
  }
}

# Security policy for external load balancer
resource "google_compute_security_policy" "public_key_service_security_policy" {
  project     = var.project_id
  name        = "${var.environment}-public-key-service-security-policy"
  description = "Security policy with for Public Key Service LB"
  type        = "CLOUD_ARMOR"

  adaptive_protection_config {
    layer_7_ddos_defense_config {
      enable = var.use_adaptive_protection
    }
  }

  rule {
    description = "Default allow all rule"
    action      = "allow"
    priority    = "2147483647"

    match {
      versioned_expr = "SRC_IPS_V1"

      config {
        src_ip_ranges = ["*"]
      }
    }
  }

  dynamic "rule" {
    for_each = var.public_key_security_policy_rules
    content {
      description = rule.value.description
      action      = rule.value.action
      priority    = rule.value.priority
      preview     = rule.value.preview

      match {
        # Basic rules handle IP addresses/ranges only and require both
        # versioned_expr and config be defined.
        versioned_expr = rule.value.match.versioned_expr

        dynamic "config" {
          for_each = rule.value.match.expr == null ? [1] : []
          content {
            src_ip_ranges = rule.value.match.config.src_ip_ranges
          }
        }

        # Advanced rules handle CEL expressions and require expr to be defined.
        dynamic "expr" {
          for_each = rule.value.match.expr != null ? [1] : []
          content {
            expression = rule.value.match.expr.expression
          }
        }
      }
    }
  }
}

# Backend service that groups network endpoint groups to Cloud Run for Load
# Balancer to use.
resource "google_compute_backend_service" "public_key_service_cloud_run" {
  name        = "${var.environment}-public-key-service-cloud-run"
  project     = var.project_id
  description = "Load balancer backend service for Public Key services."

  enable_cdn            = var.enable_get_public_key_cdn
  load_balancing_scheme = "EXTERNAL_MANAGED"

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

  security_policy = var.enable_security_policy ? google_compute_security_policy.public_key_service_security_policy.id : null
}

# URL Map creates Load balancer to Cloud Run
resource "google_compute_url_map" "public_key_service_cloud_run" {
  project         = var.project_id
  name            = "${var.environment}-public-key-service-cloud-run"
  default_service = google_compute_backend_service.public_key_service_cloud_run.id
}

# Proxy to loadbalancer for Cloud Run. HTTP without custom domain
resource "google_compute_target_http_proxy" "public_key_service_cloud_run" {
  count = !var.enable_domain_management ? 1 : 0

  project = var.project_id
  name    = "${var.environment}-public-key-service-cloud-run"
  url_map = google_compute_url_map.public_key_service_cloud_run.id
}

# Proxy to loadbalancer for Cloud Run. HTTPS with custom domain
resource "google_compute_target_https_proxy" "public_key_service_cloud_run" {
  count = var.enable_domain_management ? 1 : 0

  project = var.project_id
  name    = "${var.environment}-public-key-service-cloud-run"
  url_map = google_compute_url_map.public_key_service_cloud_run.id

  ssl_certificates = [
    google_compute_managed_ssl_certificate.get_public_key_loadbalancer[0].id,
  ]
}

# Reserve IP address.
resource "google_compute_global_address" "public_key_cloud_run" {
  project = var.project_id
  name    = "${var.environment}-public-key-cloud-run"
}

# Map IP address and loadbalancer proxy to Cloud Run
resource "google_compute_global_forwarding_rule" "public_key_service_cloud_run" {
  project               = var.project_id
  name                  = "${var.environment}-public-key-service-cloud-run"
  ip_address            = google_compute_global_address.public_key_cloud_run.address
  port_range            = var.enable_domain_management ? "443" : "80"
  load_balancing_scheme = "EXTERNAL_MANAGED"

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
