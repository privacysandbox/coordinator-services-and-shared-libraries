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

output "pbs_auth_audience_url" {
  description = "The URL by which the cloud function can be invoked."
  value       = google_cloudfunctions2_function.pbs_auth_cloudfunction.service_config[0].uri
}

output "pbs_auth_cloudfunction_name" {
  description = "The name of the auth Cloud Function."
  value       = google_cloudfunctions2_function.pbs_auth_cloudfunction.name
}
