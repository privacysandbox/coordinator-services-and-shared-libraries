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
      version = ">= 6.29.0"
    }
  }
}

resource "google_secret_manager_secret" "coordinator_parameter" {
  project   = var.project_id
  secret_id = format("scp-%s-%s", var.environment, var.parameter_name)
  replication {
    auto {}
  }
}

resource "google_secret_manager_secret_version" "coordinator_parameter_value" {
  secret      = google_secret_manager_secret.coordinator_parameter.id
  secret_data = var.parameter_value
}
