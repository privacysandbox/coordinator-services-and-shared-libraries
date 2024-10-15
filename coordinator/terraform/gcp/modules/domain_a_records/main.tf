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

# Hosted zone must exist with NS records
# See /applications/project_resources for an example
data "google_dns_managed_zone" "dns_zone" {
  count = var.enable_domain_management ? 1 : 0

  project = var.parent_domain_name_project
  name    = replace(var.parent_domain_name, ".", "-")

  lifecycle {
    # Parent domain name should not be empty
    precondition {
      condition     = var.parent_domain_name != "" && var.parent_domain_name_project != ""
      error_message = "var.enable_domain_management is true with an empty var.parent_domain_name."
    }
  }
}

# Add A record for loadbalancer IPs.
resource "google_dns_record_set" "a" {
  for_each = var.service_domain_to_address_map

  project = var.parent_domain_name_project
  # Name must end with period
  name         = "${each.key}."
  managed_zone = data.google_dns_managed_zone.dns_zone[0].name
  type         = "A"
  ttl          = 300

  rrdatas = [each.value]
}
