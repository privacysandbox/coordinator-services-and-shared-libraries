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

  # Do not modify these Spanner variable. Doing so will result in data loss as
  # the spanner database will be destroyed and recreated.
  pbs_spanner_budget_key_table_name     = "BudgetKey"
  pbs_spanner_partition_lock_table_name = "PartitionLockTable"
}

# A random hex value to ensure unique resource names.
resource "random_id" "gcs_random_suffix" {
  byte_length = 8
}

# Note: name max length = 63
resource "google_storage_bucket" "pbs_journal_bucket" {
  name                        = "${var.environment}_scp_pbs_journals_${random_id.gcs_random_suffix.hex}"
  location                    = var.region
  force_destroy               = var.pbs_cloud_storage_journal_bucket_force_destroy
  public_access_prevention    = "enforced"
  uniform_bucket_level_access = true

  versioning {
    enabled = var.pbs_cloud_storage_journal_bucket_versioning
  }

  lifecycle_rule {
    action {
      type = "Delete"
    }
    condition {
      age = 60
    }
  }

  lifecycle_rule {
    action {
      type = "Delete"
    }
    condition {
      days_since_noncurrent_time = 10
    }
  }
}

# Note: display_name max length = 30
resource "google_spanner_instance" "pbs_spanner_instance" {
  name             = "${var.environment}-pbsinst"
  display_name     = "${var.environment}-pbsinst"
  config           = local.spanner_config
  processing_units = var.pbs_spanner_autoscaling_config == null ? var.pbs_spanner_instance_processing_units : null

  dynamic "autoscaling_config" {
    for_each = var.pbs_spanner_autoscaling_config != null ? [var.pbs_spanner_autoscaling_config] : []
    content {
      autoscaling_limits {
        max_nodes = autoscaling_config.value.max_nodes
        min_nodes = autoscaling_config.value.min_nodes
      }

      autoscaling_targets {
        high_priority_cpu_utilization_percent = autoscaling_config.value.high_priority_cpu_utilization_percent
        storage_utilization_percent           = autoscaling_config.value.storage_utilization_percent
      }
    }
  }

  lifecycle {
    precondition {
      condition     = (var.pbs_spanner_instance_processing_units != null) != (var.pbs_spanner_autoscaling_config != null)
      error_message = "Exactly one of pbs_spanner_processing_units and pbs_spanner_autoscaling_config must be set."
    }
  }
}

resource "google_spanner_database" "pbs_spanner_database" {
  instance                 = google_spanner_instance.pbs_spanner_instance.name
  name                     = "${var.environment}-pbsdb"
  version_retention_period = var.pbs_spanner_database_retention_period
  deletion_protection      = var.pbs_spanner_database_deletion_protection

  # Do not modify existing DDL statements. Only append new statements to this list.
  # Modifying existing statements will force Terraform to replace the associated resources,
  # resulting in data loss.
  #
  # Example of what NOT to do:
  #
  #   Incorrect: Adding a column to an existing CREATE TABLE statement.
  #     CREATE TABLE MyTable (
  #       MyExistingColumn String(1024),
  #       MyNewColumn String(1024),  <-- This modification is incorrect
  #     ) PRIMARY KEY (MyExistingColumn)
  #
  #   This will cause Terraform to drop and recreate the table, deleting all existing data.
  #   Instead, append a new ALTER TABLE statement to add the column.
  ddl = [
    <<-EOT
    CREATE TABLE ${local.pbs_spanner_budget_key_table_name} (
      Budget_Key STRING(1024) NOT NULL,
      Timeframe STRING(1024) NOT NULL,
      Value JSON,
    )
    PRIMARY KEY (Budget_Key, Timeframe)
    EOT
    ,
    <<-EOT
    CREATE TABLE ${local.pbs_spanner_partition_lock_table_name} (
      LockId STRING(MAX) NOT NULL,
      Value JSON,
    )
    PRIMARY KEY (LockId)
    EOT
  ]
}
