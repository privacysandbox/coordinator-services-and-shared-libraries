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

# Get the hosted zone, this already exists as an variable for each project
data "google_dns_managed_zone" "dns_zone" {
  name    = replace(var.parent_domain_name, ".", "-")
  project = var.project_id

  lifecycle {
    # Parent domain name should not be null
    precondition {
      condition     = var.parent_domain_name != null && var.project_id != null
      error_message = "Domain management is enabled with an null parent_domain_name or project_id."
    }
  }
}

# A set representative of the public key alternative domains.
locals {
  alternative_domains_set = var.enable_public_key_alternative_domain ? toset(var.public_key_service_alternate_domain_names) : toset([])
}

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

# Generate random id in the name to solve name duplicate issue when reissuing the certificate
resource "random_id" "public_key_default_cert_name" {
  byte_length = 4
  prefix      = "${var.environment}-public-key-cert-default-"
  keepers = {
    domain = var.public_key_domain
  }
}

module "public_key_service_security_policy" {
  source = "../shared/cloudarmor_security_policy"

  project_id                  = var.project_id
  security_policy_name        = "${var.environment}-public-key-service-security-policy"
  security_policy_description = "Security policy for Public Key Service LB"
  use_adaptive_protection     = var.use_adaptive_protection
  ddos_thresholds             = var.public_key_ddos_thresholds
  security_policy_rules       = var.public_key_security_policy_rules
}
moved {
  from = google_compute_security_policy.public_key_service_security_policy
  to   = module.public_key_service_security_policy.google_compute_security_policy.security_policy
}

# Monitoring alerts and notifications for Cloud Armor security policies.
module "public_key_service_cloudarmor_alarms" {
  count  = var.enable_cloud_armor_alerts ? 1 : 0
  source = "../shared/cloudarmor_alarms"

  project_id              = var.project_id
  environment             = var.environment
  notifications_enabled   = var.enable_cloud_armor_notifications
  notification_channel_id = var.cloud_armor_notification_channel_id
  security_policy_name    = module.public_key_service_security_policy.security_policy.name
  service_prefix          = "${var.environment} Public Key Service"
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

  security_policy = var.enable_security_policy ? module.public_key_service_security_policy.security_policy.id : null
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

  ssl_certificates = var.disable_public_key_ssl_cert ? [] : [
    google_compute_managed_ssl_certificate.get_public_key_loadbalancer[0].id,
  ]

  certificate_map = var.enable_public_key_certificate_map ? "//certificatemanager.googleapis.com/${google_certificate_manager_certificate_map.public_key_cert_map[0].id}" : null
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
    google_compute_target_https_proxy.public_key_service_cloud_run[0].self_link :
    google_compute_target_http_proxy.public_key_service_cloud_run[0].id
  )
}

# Creates SSL cert for given domain. Terraform does not wait for SSL cert to be provisioned before the `apply` operation
# succeeds. As long as the hosted zone exists, it can take up to 20 mins for the cert to be provisioned.
# See console for status: https://console.cloud.google.com/loadbalancing/advanced/sslCertificates/list
# Note: even if status of cert becomes 'Active', it can still take around 10 mins for requests to the domain to work.
resource "google_compute_managed_ssl_certificate" "get_public_key_loadbalancer" {
  count   = var.enable_domain_management && !var.remove_public_key_ssl_cert ? 1 : 0
  project = var.project_id
  name    = "${var.environment}-public-key-cert"

  managed {
    domains = concat([var.public_key_domain], var.public_key_service_alternate_domain_names)
  }
}

# Mapping on all the public domains to its certificate.
resource "google_certificate_manager_certificate_map" "public_key_cert_map" {
  count       = var.enable_domain_management ? 1 : 0
  project     = var.project_id
  name        = "${var.project_id}-global-public-key-cert-map"
  description = "Certificate map for public key"
}

# DNS auth for the public key default domain.
resource "google_certificate_manager_dns_authorization" "public_key_dns_auth_default" {
  count       = var.enable_domain_management ? 1 : 0
  project     = var.project_id
  name        = "${var.environment}-public-key-dns-auth-default"
  description = "DNS authorization for GCP public key default domain validation"
  domain      = var.public_key_domain
}

#  Add the default DNS auth record.
resource "google_dns_record_set" "public_key_auth_record_default" {
  count        = var.enable_domain_management ? 1 : 0
  project      = var.project_id
  name         = google_certificate_manager_dns_authorization.public_key_dns_auth_default[0].dns_resource_record[0].name
  type         = google_certificate_manager_dns_authorization.public_key_dns_auth_default[0].dns_resource_record[0].type
  ttl          = 60
  managed_zone = data.google_dns_managed_zone.dns_zone.name
  rrdatas      = [google_certificate_manager_dns_authorization.public_key_dns_auth_default[0].dns_resource_record[0].data]
}

# Cert for public key default record.
resource "google_certificate_manager_certificate" "public_key_loadbalancer_cert_default" {
  count       = var.enable_domain_management ? 1 : 0
  project     = var.project_id
  name        = random_id.public_key_default_cert_name.hex
  description = "Cert manager cert for the default public key endpoint"

  managed {
    domains = compact([
      google_certificate_manager_dns_authorization.public_key_dns_auth_default[0].domain,
    ])
    dns_authorizations = compact([
      google_certificate_manager_dns_authorization.public_key_dns_auth_default[0].id,
    ])
  }
  lifecycle {
    create_before_destroy = true
  }
}

# Cert map entry for the public key default record.
resource "google_certificate_manager_certificate_map_entry" "default" {
  count        = var.enable_domain_management ? 1 : 0
  project      = var.project_id
  name         = "${var.project_id}-public-key-cert-map-entry-default"
  description  = "Public key Certificate Map entry for the default domain"
  map          = google_certificate_manager_certificate_map.public_key_cert_map[0].name
  certificates = [google_certificate_manager_certificate.public_key_loadbalancer_cert_default[0].id]
  matcher      = "PRIMARY"
}

# A random id as suffix of cert name to solve name duplicate issue when reissuing the certificate.
resource "random_id" "alternative_cert_suffix" {
  for_each    = local.alternative_domains_set
  byte_length = 4
}

# Generate DNS auth for each alternative public key domains.
resource "google_certificate_manager_dns_authorization" "alternative_authorization" {
  for_each    = local.alternative_domains_set
  project     = var.project_id
  name        = "${var.environment}-public-key-alt-dns-auth-${random_id.alternative_cert_suffix[each.value].hex}"
  description = "DNS authorization for GCP public key alternative domains validation"
  domain      = each.value
}

# Create certificate for each alternative public key domains.
resource "google_certificate_manager_certificate" "alternative_cert" {
  for_each    = local.alternative_domains_set
  project     = var.project_id
  name        = "${var.environment}-public-key-alt-${random_id.alternative_cert_suffix[each.value].hex}"
  description = "Google-managed cert for ${each.value}"

  managed {
    domains = [each.value]
    dns_authorizations = [
      google_certificate_manager_dns_authorization.alternative_authorization[each.value].id
    ]
  }

  lifecycle {
    create_before_destroy = true
  }
}

resource "google_certificate_manager_certificate_map_entry" "alternative_entries" {
  for_each = local.alternative_domains_set

  project      = var.project_id
  name         = "${var.project_id}-public-key-map-entry-alt-${random_id.alternative_cert_suffix[each.value].hex}"
  description  = "Public key Certificate Map entry for the alternative domains"
  map          = google_certificate_manager_certificate_map.public_key_cert_map[0].name
  certificates = [google_certificate_manager_certificate.alternative_cert[each.value].id]
  hostname     = each.value
}

