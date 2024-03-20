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
  name             = "${var.environment}-keydbinstance"
  display_name     = "${var.environment}-keydbinstance"
  config           = var.spanner_instance_config
  processing_units = var.spanner_processing_units
}

resource "google_spanner_database" "keydb" {
  instance                 = google_spanner_instance.keydb_instance.name
  name                     = "${var.environment}-keydb"
  version_retention_period = var.key_db_retention_period
  ddl = [
    <<-EOT
    CREATE TABLE KeySets (
      KeyId STRING(50) NOT NULL,
      PublicKey STRING(1000) NOT NULL,
      PrivateKey STRING(1000) NOT NULL,
      PublicKeyMaterial STRING(500) NOT NULL,
      KeySplitData JSON,
      KeyType STRING(500) NOT NULL,
      KeyEncryptionKeyUri STRING(1000) NOT NULL,
      ExpiryTime TIMESTAMP NOT NULL,
      TtlTime TIMESTAMP NOT NULL,
      CreatedAt TIMESTAMP NOT NULL OPTIONS (allow_commit_timestamp=true),
      UpdatedAt TIMESTAMP NOT NULL OPTIONS (allow_commit_timestamp=true))
    PRIMARY KEY (KeyId),
    ROW DELETION POLICY (OLDER_THAN(TtlTime, INTERVAL 0 DAY))
  EOT
    , "CREATE INDEX KeySetsByExpiryTime ON KeySets(ExpiryTime)"
    , "ALTER TABLE KeySets ADD COLUMN ActivationTime TIMESTAMP"
    , "CREATE INDEX KeySetsByExpiryActivationDesc ON KeySets(ExpiryTime DESC, ActivationTime DESC)"
    , "DROP INDEX KeySetsByExpiryTime"
  ]

  deletion_protection = true
}
