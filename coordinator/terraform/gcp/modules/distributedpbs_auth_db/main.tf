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

locals {
  spanner_config = "regional-${var.region}"

  # Do not modify this Spanner variable. Doing so will result in data loss as
  # the spanner database will be destroyed and recreated.
  pbs_auth_v2_spanner_table_name = "AuthorizationV2"
}

# Note: display_name max length = 30
resource "google_spanner_instance" "pbs_auth_spanner_instance" {
  name             = "${var.environment}-pbsauthinst"
  display_name     = "${var.environment}-pbsauthinst"
  config           = local.spanner_config
  processing_units = var.pbs_auth_spanner_instance_processing_units
}

resource "google_spanner_database" "pbs_auth_spanner_database" {
  instance                 = google_spanner_instance.pbs_auth_spanner_instance.name
  name                     = "${var.environment}-pbsauthdb"
  version_retention_period = var.pbs_auth_spanner_database_retention_period
  deletion_protection      = var.pbs_auth_spanner_database_deletion_protection

  # Do not remove DDL statements. You may only append.
  # Terraform apply will replace these resources otherwise.
  ddl = [
    <<-EOT
    CREATE TABLE ${local.pbs_auth_v2_spanner_table_name} (
      AccountId STRING(MAX),
      AdtechSites ARRAY<STRING(MAX)>,
    )
    PRIMARY KEY (AccountId)
    EOT
  ]
}
