# Copyright 2025 Google LLC
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
    google = ">= 6.29.0"
  }
}

resource "google_logging_metric" "spanner_scheduled_backups" {
  count = var.enabled_metrics.spanner_scheduled_backups ? 1 : 0

  project = var.project_id
  name    = "ps/spanner_scheduled_backups"
  filter  = <<EOT
resource.type="spanner_instance"
protoPayload.methodName="CreateScheduledBackup"
operation.last="true"
EOT

  description = "Count of backups created per Spanner backup schedule."
  label_extractors = {
    "backupSchedule" = "EXTRACT(protoPayload.metadata.backupSchedules)"
  }

  metric_descriptor {
    metric_kind  = "DELTA"
    value_type   = "INT64"
    display_name = "Spanner scheduled backup count"

    labels {
      key         = "backupSchedule"
      description = "Backup schedule ID"
      value_type  = "STRING"
    }
  }
}
