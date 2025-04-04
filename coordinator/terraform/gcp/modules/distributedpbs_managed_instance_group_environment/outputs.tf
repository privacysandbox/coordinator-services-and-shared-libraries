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

output "pbs_instance_group_url" {
  description = "The URL of the instance group created by the manager."
  value       = google_compute_region_instance_group_manager.pbs_instance_group.instance_group
}

output "pbs_cloud_run_name" {
  description = "The name of the Cloud Run PBS"
  value       = var.deploy_pbs_cloud_run ? google_cloud_run_v2_service.pbs_instance[0].name : ""
}
