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

# Get the hosted zone. This must already exist.
data "google_dns_managed_zone" "dns_zone" {
  name    = replace(var.parent_domain_name, ".", "-")
  project = var.parent_domain_project

  lifecycle {
    # Parent domain name should not be empty
    precondition {
      condition     = var.parent_domain_name != "" && var.parent_domain_project != ""
      error_message = "Domain management is enabled with an empty parent_domain_name or parent_domain_project."
    }
  }
}

resource "google_dns_record_set" "pbs_a_record" {
  project = var.parent_domain_project
  # Name must end with a period
  name         = "${var.pbs_domain}."
  managed_zone = data.google_dns_managed_zone.dns_zone.name
  type         = "A"
  ttl          = 300

  rrdatas = [var.pbs_ip_address]
}
