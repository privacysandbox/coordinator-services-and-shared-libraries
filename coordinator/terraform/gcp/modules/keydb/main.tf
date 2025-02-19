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

resource "google_spanner_instance" "keydb_instance" {
  project          = var.project_id
  name             = "${var.environment}-keydbinstance"
  display_name     = "${var.environment}-keydbinstance"
  config           = var.spanner_instance_config
  processing_units = var.spanner_processing_units
}

resource "google_spanner_database" "keydb" {
  project                  = var.project_id
  instance                 = google_spanner_instance.keydb_instance.name
  name                     = "${var.environment}-keydb"
  version_retention_period = var.key_db_retention_period

  # Do NOT modify existing DDL files. Terraform would recreate the database, causing data loss.
  # Instead, create a file containing the new DDL statements and append it to this list.
  ddl = [
    file("${path.module}/files/01_create_keysets_table.sql"),
    trimspace(file("${path.module}/files/02_create_expiry_time_index.sql")),
    trimspace(file("${path.module}/files/03_add_activation_time_column.sql")),
    trimspace(file("${path.module}/files/04_create_activation_expiry_index.sql")),
    trimspace(file("${path.module}/files/05_drop_expiry_time_index.sql")),
  ]

  deletion_protection = true
}
