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

output "journal_s3_bucket_name" {
  value = aws_s3_bucket.pbs_journal_bucket.bucket
}

output "budget_keys_dynamo_db_table_name" {
  value = aws_dynamodb_table.budget_keys_table.name
}

output "partition_lock_dynamo_db_table_name" {
  value = aws_dynamodb_table.partition_lock_table.name
}
