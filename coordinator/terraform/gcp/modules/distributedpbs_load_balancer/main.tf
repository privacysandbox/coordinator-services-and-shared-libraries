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

# NOTE:
# If domain management is enabled, we use an application load balancer (via https proxy)
# which wraps both the cloud function and the managed instance group.
# If it is disabled, then we use a network load balancer (via tcp proxy) which
# only wraps the managed instance group.

locals {
  # IP CIDR ranges for the load balancer and health check services owned by Google
  # https://cloud.google.com/load-balancing/docs/https/setting-up-https#configuring_firewall_rules
  lb_ip1 = "130.211.0.0/22"
  lb_ip2 = "35.191.0.0/16"
}

# Setup the firewall rule for health-check and LB incoming traffic
resource "google_compute_firewall" "load_balancer_firewall" {
  project     = var.project_id
  name        = "${var.environment}-pbs-lb-firewall"
  description = "Firewall rule to allow the health check and main service ports for PBS environment ${var.environment}"
  network     = var.pbs_vpc_network_id

  # Allow requests from these IP address ranges
  source_ranges = [
    local.lb_ip1,
    local.lb_ip2,
    var.pbs_ip_address
  ]

  # Only allow requests to instances that contain this target tag
  # The instance or instance template must have this tag added
  target_tags = [var.pbs_instance_target_tag]

  # Allow the health check and main service port
  allow {
    protocol = "tcp"
    ports    = [var.pbs_health_check_port, var.pbs_main_port]
  }
}

resource "google_compute_firewall" "ssh_firewall" {
  count   = var.pbs_instance_allow_ssh ? 1 : 0
  project = var.project_id
  name    = "${var.environment}-allow-ssh"
  network = var.pbs_vpc_network_id

  source_ranges = [
    "0.0.0.0/0"
  ]

  # Only allow requests to instances that contain this target tag
  # The instance or instance template must have this tag added
  target_tags = [var.pbs_instance_target_tag]

  allow {
    protocol = "tcp"
    ports    = [22]
  }
}

# Network Endpoint Group to route to auth cloud function
resource "google_compute_region_network_endpoint_group" "pbs_auth_network_endpoint_group" {
  count                 = var.enable_domain_management ? 1 : 0
  name                  = "${var.environment}-${var.region}-pbs-auth"
  network_endpoint_type = "SERVERLESS"
  region                = var.region
  cloud_run {
    service = var.pbs_auth_cloudfunction_name
  }
}

# Backend service that maps to the auth cloud function for the load balancer to use
resource "google_compute_backend_service" "pbs_auth_loadbalancer_backend" {
  count                           = var.enable_domain_management ? 1 : 0
  name                            = "${var.environment}-pbs-auth-backend"
  project                         = var.project_id
  description                     = "Backend service to point to PBS auth Clound Function"
  enable_cdn                      = false
  protocol                        = "HTTP2"
  connection_draining_timeout_sec = 30

  backend {
    description = var.environment
    group       = google_compute_region_network_endpoint_group.pbs_auth_network_endpoint_group[0].id
  }

  log_config {
    enable = true
  }
}

# Backend service that maps to the PBS managed instance group for the load balancer to use
resource "google_compute_backend_service" "pbs_backend_service" {
  name                            = "${var.environment}-pbs-backend"
  project                         = var.project_id
  description                     = "Backend service to point to PBS managed instance group"
  enable_cdn                      = false
  protocol                        = var.enable_domain_management ? "HTTP2" : "TCP"
  port_name                       = var.pbs_named_port
  timeout_sec                     = 3600
  connection_draining_timeout_sec = 30

  health_checks = [google_compute_health_check.pbs_health_check.id]

  backend {
    description    = var.environment
    group          = var.pbs_managed_instance_group_url
    balancing_mode = "UTILIZATION"
  }

  log_config {
    enable = true
  }
}

resource "google_compute_health_check" "pbs_health_check" {
  name    = "${var.environment}-pbs-health-check"
  project = var.project_id

  check_interval_sec  = 10
  timeout_sec         = 10
  unhealthy_threshold = 5

  # Enable when domain management is enabled
  dynamic "http2_health_check" {
    for_each = var.enable_domain_management ? [1] : []
    content {
      request_path       = "/health"
      port               = var.pbs_health_check_port
      port_specification = "USE_FIXED_PORT"
      proxy_header       = "NONE"
      host               = var.pbs_ip_address
    }
  }

  # Enabled when domain management is NOT enabled
  dynamic "tcp_health_check" {
    for_each = var.enable_domain_management ? [] : [1]
    content {
      port               = var.pbs_health_check_port
      port_specification = "USE_FIXED_PORT"
      proxy_header       = "NONE"
    }
  }
}

# The URL map creates the HTTP(S) LB
resource "google_compute_url_map" "pbs_load_balancer" {
  count   = var.enable_domain_management ? 1 : 0
  name    = "${var.environment}-pbs-lb"
  project = var.project_id

  host_rule {
    hosts        = ["*"]
    path_matcher = "${var.environment}-allowed"
  }

  # Return a 301 for requests that go to an unmatched path
  default_url_redirect {
    https_redirect         = true
    redirect_response_code = "MOVED_PERMANENTLY_DEFAULT"
    strip_query            = false
  }

  path_matcher {
    name = "${var.environment}-allowed"

    # Return a 301 for requests that go to an unmatched path
    default_url_redirect {
      https_redirect         = true
      redirect_response_code = "MOVED_PERMANENTLY_DEFAULT"
      strip_query            = false
    }

    path_rule {
      paths = [
        "/v1/transactions:begin",
        "/v1/transactions:prepare",
        "/v1/transactions:commit",
        "/v1/transactions:notify",
        "/v1/transactions:abort",
        "/v1/transactions:end",
        "/v1/transactions:status"
      ]
      service = google_compute_backend_service.pbs_backend_service.id
    }

    path_rule {
      paths = [
        "/v1/auth"
      ]
      service = google_compute_backend_service.pbs_auth_loadbalancer_backend[0].id
    }
  }
}

# Proxy to loadbalancer. TCP without custom domain
resource "google_compute_target_tcp_proxy" "pbs_loadbalancer_proxy" {
  project         = var.project_id
  count           = var.enable_domain_management ? 0 : 1
  name            = "${var.environment}-pbs-proxy"
  backend_service = google_compute_backend_service.pbs_backend_service.id
}

# Proxy to loadbalancer. HTTPS with custom domain
resource "google_compute_target_https_proxy" "pbs_loadbalancer_proxy" {
  project = var.project_id
  count   = var.enable_domain_management ? 1 : 0
  name    = "${var.environment}-pbs-proxy"
  url_map = google_compute_url_map.pbs_load_balancer[0].id

  ssl_certificates = [
    google_compute_managed_ssl_certificate.pbs_loadbalancer[0].id
  ]
}

# Map IP address and loadbalancer proxy
resource "google_compute_global_forwarding_rule" "pbs_loadbalancer_config" {
  project    = var.project_id
  name       = "${var.environment}-pbs-frontend-configuration"
  ip_address = var.pbs_ip_address
  port_range = "443"
  target = (var.enable_domain_management ? google_compute_target_https_proxy.pbs_loadbalancer_proxy[0].id
  : google_compute_target_tcp_proxy.pbs_loadbalancer_proxy[0].id)
}

# Creates SSL cert for given domain. Terraform does not wait for SSL cert to be provisioned before the `apply` operation
# succeeds. As long as the hosted zone exists, it can take up to 20 mins for the cert to be provisioned.
# See console for status: https://console.cloud.google.com/loadbalancing/advanced/sslCertificates/list
# Note: even if status of cert becomes 'Active', it can still take around 10 mins for requests to the domain to work.
resource "google_compute_managed_ssl_certificate" "pbs_loadbalancer" {
  count   = var.enable_domain_management ? 1 : 0
  project = var.project_id
  name    = "${var.environment}-pbs-cert"

  lifecycle {
    create_before_destroy = true
  }

  managed {
    domains = [var.pbs_domain]
  }
}
