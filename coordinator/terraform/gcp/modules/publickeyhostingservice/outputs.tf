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

output "get_public_key_loadbalancer_ip" {
  value = local.use_cloud_function ? google_compute_global_address.get_public_key_ip_address[0].address : null
}

output "public_key_cloud_run_loadbalancer_ip" {
  value = google_compute_global_address.public_key_cloud_run.address
}
