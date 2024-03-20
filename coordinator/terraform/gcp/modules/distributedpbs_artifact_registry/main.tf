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

# TODO: Look into "Container analysis and vulnerability scanning" for GCP
#       as an alternative to AWS image scanning

# Create Google Artifact Registry (AR) repository. Max id length is 64 chars.
resource "google_artifact_registry_repository" "artifact_registry_repository" {
  location      = var.region
  repository_id = "${var.environment}-scp-pbs-artifact-registry-repo"
  format        = "DOCKER"
  description   = "The repository for storing PBS docker images."
}
