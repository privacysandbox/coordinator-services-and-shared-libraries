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

data "google_dns_managed_zone" "dns_zone" {
  name    = replace(var.parent_domain_name, ".", "-")
  project = var.parent_domain_name_project

  lifecycle {
    # Parent domain name should not be empty
    precondition {
      condition     = var.parent_domain_name != null && var.parent_domain_name_project != null
      error_message = "Domain management is enabled with an empty parent_domain_name or parent_domain_name_project."
    }
  }
}

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
  ddos_thresholds             = var.encryption_key_ddos_thresholds
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

  ssl_certificates = var.disable_private_key_service_compute_engine_ssl_cert ? [] : [
    google_compute_managed_ssl_certificate.encryption_key_service_loadbalancer[0].id,
  ]

  certificate_map = (
    var.enable_private_key_service_certificate_map ?
    "//certificatemanager.googleapis.com/${google_certificate_manager_certificate_map.encryption_key_cert_map[0].id}" :
    null
  )
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
  count   = var.enable_domain_management && !var.remove_private_key_service_compute_engine_ssl_cert ? 1 : 0
  project = var.project_id
  name    = "${var.environment}-encryption-key-cert"

  managed {
    domains = [var.encryption_key_domain]
  }
}

# Mapping on all the private domains to its certificate.
resource "google_certificate_manager_certificate_map" "encryption_key_cert_map" {
  count       = var.enable_domain_management ? 1 : 0
  project     = var.project_id
  name        = "${var.project_id}-ekey-cert-map"
  description = "Certificate map for private key"
}

# DNS auth for the private key default domain.
resource "google_certificate_manager_dns_authorization" "encryption_key_dns_auth_default" {
  count       = var.enable_domain_management ? 1 : 0
  project     = var.project_id
  name        = "${var.environment}-ekey-dns-auth-default"
  description = "DNS authorization for GCP encryption key default domain validation"
  domain      = var.encryption_key_domain
}

#  Add the default DNS auth record.
resource "google_dns_record_set" "encryption_key_auth_record_default" {
  count        = var.enable_domain_management ? 1 : 0
  project      = var.parent_domain_name_project
  name         = google_certificate_manager_dns_authorization.encryption_key_dns_auth_default[0].dns_resource_record[0].name
  type         = google_certificate_manager_dns_authorization.encryption_key_dns_auth_default[0].dns_resource_record[0].type
  ttl          = 60
  managed_zone = data.google_dns_managed_zone.dns_zone.name
  rrdatas      = [google_certificate_manager_dns_authorization.encryption_key_dns_auth_default[0].dns_resource_record[0].data]
}

# Generate random id in the name to solve name duplicate issue when reissuing the certificate
resource "random_id" "encryption_key_default_cert_name" {
  byte_length = 4
  prefix      = "${var.environment}-ekey-cert-default-"
  keepers = {
    domain = var.encryption_key_domain
  }
}

# Cert for private key default record.
resource "google_certificate_manager_certificate" "encryption_key_loadbalancer_cert_default" {
  count       = var.enable_domain_management ? 1 : 0
  project     = var.project_id
  name        = random_id.encryption_key_default_cert_name.hex
  description = "Certificate manager cert for the default private key endpoint"

  managed {
    domains = compact([
      google_certificate_manager_dns_authorization.encryption_key_dns_auth_default[0].domain,
    ])
    dns_authorizations = compact([
      google_certificate_manager_dns_authorization.encryption_key_dns_auth_default[0].id,
    ])
  }
  lifecycle {
    create_before_destroy = true
  }
}

# Cert map entry for the private key default record.
resource "google_certificate_manager_certificate_map_entry" "default" {
  count        = var.enable_domain_management ? 1 : 0
  project      = var.project_id
  name         = "${var.project_id}-ekey-cert-map-entry-default" # must be globally unique and <= 63 chars
  description  = "Private key Certificate Map entry for the default domain"
  map          = google_certificate_manager_certificate_map.encryption_key_cert_map[0].name
  certificates = [google_certificate_manager_certificate.encryption_key_loadbalancer_cert_default[0].id]
  matcher      = "PRIMARY"
}

# A random id as suffix of cert name to solve name duplicate issue when reissuing the certificate.
resource "random_id" "alternative_cert_suffix" {
  for_each    = toset(var.private_key_service_alternate_domain_names)
  byte_length = 4
}

# Generate DNS auth for each alternative private key domains.
resource "google_certificate_manager_dns_authorization" "alternative_authorization" {
  for_each    = toset(var.private_key_service_alternate_domain_names)
  project     = var.project_id
  name        = "${var.environment}-ekey-alt-dns-auth-${random_id.alternative_cert_suffix[each.value].hex}"
  description = "DNS authorization for GCP private key alternative domains validation"
  domain      = each.value
}

# Create certificate for each alternative private key domains.
resource "google_certificate_manager_certificate" "alternative_cert" {
  for_each    = toset(var.private_key_service_alternate_domain_names)
  project     = var.project_id
  name        = "${var.environment}-ekey-alt-${random_id.alternative_cert_suffix[each.value].hex}"
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
  for_each = toset(var.private_key_service_alternate_domain_names)

  project      = var.project_id
  name         = "${var.project_id}-ekey-cert-map-entry-${random_id.alternative_cert_suffix[each.value].hex}" # must be globally unique and <= 63 chars
  description  = "Private key Certificate Map entry for the alternative domains"
  map          = google_certificate_manager_certificate_map.encryption_key_cert_map[0].name
  certificates = [google_certificate_manager_certificate.alternative_cert[each.value].id]
  hostname     = each.value
}
