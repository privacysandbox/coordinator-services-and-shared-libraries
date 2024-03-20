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

module "lb-http_serverless_negs" {
  source  = "GoogleCloudPlatform/lb-http/google//modules/serverless_negs"
  version = "~> 6.1"
  name    = "${var.environment}-${var.load_balancer_name}"
  project = var.project_id

  # Creates SSL cert for given domain. Terraform does not wait for SSL cert to be provisioned before the `apply` operation
  # succeeds. As long as the hosted zone exists, it can take up to 20 mins for the cert to be provisioned.
  # See console for status: https://console.cloud.google.com/loadbalancing/advanced/sslCertificates/list
  # Note: even if status of cert becomes 'Active', it can still take around 10 mins for requests to the domain to work.
  ssl                             = var.enable_domain_management
  managed_ssl_certificate_domains = [var.key_storage_domain]

  http_forward   = !var.enable_domain_management
  https_redirect = false

  backends = {
    default = {
      description             = null
      enable_cdn              = false
      security_policy         = null
      custom_request_headers  = null
      custom_response_headers = null

      groups = [
        {
          group = google_compute_region_network_endpoint_group.key_storage_endpoint_group.id
        }
      ]

      iap_config = {
        enable               = false
        oauth2_client_id     = null
        oauth2_client_secret = null
      }

      log_config = {
        enable      = false
        sample_rate = null
      }
    }
  }
}

resource "google_compute_region_network_endpoint_group" "key_storage_endpoint_group" {
  name                  = "${var.environment}-keystoragegroup"
  network_endpoint_type = "SERVERLESS"
  region                = var.region
  cloud_run {
    service = google_cloudfunctions2_function.key_storage_cloudfunction.name
  }
}
