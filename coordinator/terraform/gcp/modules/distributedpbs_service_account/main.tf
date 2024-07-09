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

terraform {
  required_providers {
    google = {
      source  = "hashicorp/google"
      version = ">= 4.36"
    }
  }
}

resource "google_service_account" "pbs_service_account" {
  # Service account id has a 30 character limit
  account_id   = "${var.environment}-pbs-mig-sa"
  display_name = "The PBS service account of the managed instance group for ${var.environment}."
}

# Allow PBS to create ID Tokens as itself,
# for authenticating requests to OpenTelemetry Collector in Cloud Run.
data "google_iam_policy" "pbs_service_account" {
  binding {
    role = "roles/iam.serviceAccountOpenIdTokenCreator"
    members = [
      # google provider version 4.36 doesn't have attribute "member" yet
      # google_service_account.pbs_service_account.member,
      "serviceAccount:${google_service_account.pbs_service_account.email}",
    ]
  }
}

resource "google_service_account_iam_policy" "pbs_service_account" {
  service_account_id = google_service_account.pbs_service_account.name
  policy_data        = data.google_iam_policy.pbs_service_account.policy_data
}
