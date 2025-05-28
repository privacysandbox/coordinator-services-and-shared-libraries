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

output "public_key_service_cloud_run_loadbalancer_ip" {
  value = google_compute_global_address.public_key_cloud_run.address
}

output "dns_authorization_cname_records_for_alternative_domains" {
  description = "DNS auth CNAME record for alternative domains, which need to be added to target hosted zones."
  value = {
    for domain, auth in google_certificate_manager_dns_authorization.alternative_authorization :
    domain => {
      name = auth.dns_resource_record[0].name
      type = auth.dns_resource_record[0].type
      data = auth.dns_resource_record[0].data
    }
  }
}
