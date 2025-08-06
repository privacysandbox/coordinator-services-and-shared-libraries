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

# Get the hosted zone, this already exists as an variable for each project
data "google_dns_managed_zone" "dns_zone" {
  name    = replace(var.parent_domain_name, ".", "-")
  project = var.parent_domain_name_project

  lifecycle {
    # Parent domain name should not be null
    precondition {
      condition     = var.parent_domain_name != null && var.parent_domain_name_project != null
      error_message = "Domain management is enabled with an null parent_domain_name or parent_domain_name_project."
    }
  }
}

# Network Endpoint Group to route to Cloud Run
resource "google_compute_region_network_endpoint_group" "key_storage_service_cloud_run" {
  project               = var.project_id
  name                  = "${var.environment}-key-storage-service-cloud-run"
  network_endpoint_type = "SERVERLESS"
  region                = var.region
  cloud_run {
    service = google_cloud_run_v2_service.key_storage_service.name
  }
}

module "key_storage_service_security_policy" {
  source = "../shared/cloudarmor_security_policy"

  project_id                  = var.project_id
  security_policy_name        = "${var.environment}-key-storage-service-security-policy"
  security_policy_description = "Security policy for Key Storage Service LB"
  use_adaptive_protection     = var.use_adaptive_protection
  ddos_thresholds             = var.key_storage_ddos_thresholds
  security_policy_rules       = var.key_storage_security_policy_rules
}
moved {
  from = google_compute_security_policy.key_storage_service_security_policy
  to   = module.key_storage_service_security_policy.google_compute_security_policy.security_policy
}

# Monitoring alerts and notifications for Cloud Armor security policies.
module "key_storage_service_cloudarmor_alarms" {
  count  = var.enable_cloud_armor_alerts ? 1 : 0
  source = "../shared/cloudarmor_alarms"

  project_id              = var.project_id
  environment             = var.environment
  notifications_enabled   = var.enable_cloud_armor_notifications
  notification_channel_id = var.cloud_armor_notification_channel_id
  security_policy_name    = module.key_storage_service_security_policy.security_policy.name
  service_prefix          = "${var.environment} Key Storage Service"
}

# Backend service that groups network endpoint groups to Cloud Run for Load
# Balancer to use.
resource "google_compute_backend_service" "key_storage_service_cloud_run" {
  project               = var.project_id
  name                  = "${var.environment}-key-storage-service-cloud-run"
  description           = "Load balancer backend service for Key Storage services."
  load_balancing_scheme = "EXTERNAL_MANAGED"

  backend {
    group = google_compute_region_network_endpoint_group.key_storage_service_cloud_run.id
  }

  security_policy = var.enable_security_policy ? module.key_storage_service_security_policy.security_policy.id : null
}

# URL Map creates Load balancer to Cloud Run
resource "google_compute_url_map" "key_storage_service_cloud_run" {
  project         = var.project_id
  name            = "${var.environment}-key-storage-service-cloud-run"
  default_service = google_compute_backend_service.key_storage_service_cloud_run.id
}

# Proxy to loadbalancer for Cloud Run. HTTP without custom domain
resource "google_compute_target_http_proxy" "key_storage_service_cloud_run" {
  count = !var.enable_domain_management ? 1 : 0

  name    = "${var.environment}-key-storage-service-cloud-run"
  url_map = google_compute_url_map.key_storage_service_cloud_run.id
}

# Proxy to loadbalancer for Cloud Run. HTTPS with custom domain
resource "google_compute_target_https_proxy" "key_storage_service_cloud_run" {
  count = var.enable_domain_management ? 1 : 0

  project = var.project_id
  name    = "${var.environment}-key-storage-service-cloud-run"
  url_map = google_compute_url_map.key_storage_service_cloud_run.id

  ssl_certificates = var.disable_key_storage_service_compute_engine_ssl_cert ? [] : [
    google_compute_managed_ssl_certificate.key_storage[0].id
  ]
  certificate_map = (
    var.enable_key_storage_service_certificate_map ?
    "//certificatemanager.googleapis.com/${google_certificate_manager_certificate_map.key_storage_cert_map[0].id}" :
    null
  )
}

resource "google_compute_global_address" "key_storage_service_cloud_run" {
  project = var.project_id
  name    = "${var.environment}-key-storage-service-cloud-run"
}

# Map IP address and loadbalancer proxy to Cloud Run
resource "google_compute_global_forwarding_rule" "key_storage_service_cloud_run" {
  project               = var.project_id
  name                  = "${var.environment}-key-storage-service-cloud-run"
  ip_address            = google_compute_global_address.key_storage_service_cloud_run.address
  port_range            = var.enable_domain_management ? "443" : "80"
  load_balancing_scheme = "EXTERNAL_MANAGED"

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

# Generate random id in the name to solve name duplicate issue when reissuing the certificate
resource "random_id" "key_storage_default_cert_name" {
  byte_length = 4
  prefix      = "${var.environment}-kstor-cert-default-"

  keepers = {
    domain = var.key_storage_domain
  }
}

# Mapping on all the key storage domains to its certificate.
resource "google_certificate_manager_certificate_map" "key_storage_cert_map" {
  count       = var.enable_domain_management ? 1 : 0
  project     = var.project_id
  name        = "${var.project_id}-kstor-cert-map" # must be globally unique and <= 63 chars
  description = "Certificate map for key storage service"
}

# DNS auth for the key storage default domain.
resource "google_certificate_manager_dns_authorization" "key_storage_dns_auth_default" {
  count       = var.enable_domain_management ? 1 : 0
  project     = var.project_id
  name        = "${var.environment}-kstor-dns-auth-default"
  description = "DNS authorization for GCP key storage default domain validation"
  domain      = var.key_storage_domain
}

#  Add the default DNS auth record.
resource "google_dns_record_set" "key_storage_auth_record_default" {
  count        = var.enable_domain_management ? 1 : 0
  project      = var.parent_domain_name_project
  name         = google_certificate_manager_dns_authorization.key_storage_dns_auth_default[0].dns_resource_record[0].name
  type         = google_certificate_manager_dns_authorization.key_storage_dns_auth_default[0].dns_resource_record[0].type
  ttl          = 60
  managed_zone = data.google_dns_managed_zone.dns_zone.name
  rrdatas      = [google_certificate_manager_dns_authorization.key_storage_dns_auth_default[0].dns_resource_record[0].data]
}

# Cert for key storage default record.
resource "google_certificate_manager_certificate" "key_storage_loadbalancer_cert_default" {
  count       = var.enable_domain_management ? 1 : 0
  project     = var.project_id
  name        = random_id.key_storage_default_cert_name.hex
  description = "Certificate manager cert for the default key storage endpoint"

  managed {
    domains = compact([
      google_certificate_manager_dns_authorization.key_storage_dns_auth_default[0].domain,
    ])
    dns_authorizations = compact([
      google_certificate_manager_dns_authorization.key_storage_dns_auth_default[0].id,
    ])
  }
  lifecycle {
    create_before_destroy = true
  }
}

# Cert map entry for the key storage default record.
resource "google_certificate_manager_certificate_map_entry" "default" {
  count        = var.enable_domain_management ? 1 : 0
  project      = var.project_id
  name         = "${var.project_id}-kstor-cert-map-entry-default" # must be globally unique and <= 63 chars
  description  = "Key Storage Certificate Map entry for the default domain"
  map          = google_certificate_manager_certificate_map.key_storage_cert_map[0].name
  certificates = [google_certificate_manager_certificate.key_storage_loadbalancer_cert_default[0].id]
  matcher      = "PRIMARY"
}
