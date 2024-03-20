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

output "pbs_cloud_storage_journal_bucket_name" {
  value = google_storage_bucket.pbs_journal_bucket.name
}

output "pbs_spanner_database_name" {
  value       = google_spanner_database.pbs_spanner_database.name
  description = "Name of the PBS Spanner database."
}

output "pbs_spanner_instance_name" {
  value       = google_spanner_instance.pbs_spanner_instance.name
  description = "Name of the PBS Spanner instance."
}

output "pbs_spanner_budget_key_table_name" {
  value       = local.pbs_spanner_budget_key_table_name
  description = "Name of the PBS budget key table"
}

output "pbs_spanner_partition_lock_table_name" {
  value       = local.pbs_spanner_partition_lock_table_name
  description = "Name of the PBS partition lock table"
}
