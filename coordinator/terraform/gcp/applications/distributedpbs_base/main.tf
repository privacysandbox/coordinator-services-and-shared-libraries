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

module "artifact_registry_repository" {
  source      = "../../modules/distributedpbs_artifact_registry"
  project_id  = var.project
  region      = var.region
  environment = var.environment
}

module "pbs_service_account" {
  source      = "../../modules/distributedpbs_service_account"
  project_id  = var.project
  environment = var.environment
}
