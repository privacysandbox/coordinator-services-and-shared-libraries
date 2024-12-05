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
      version = ">= 5.37.0"
    }
  }
}

provider "google" {
  region  = var.region
  project = var.project
}

module "artifact_registry_repository" {
  source      = "../../modules/distributedpbs_artifact_registry"
  region      = var.region
  environment = var.environment
}

module "pbs_service_account" {
  source      = "../../modules/distributedpbs_service_account"
  region      = var.region
  environment = var.environment
}
