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

output "auth_dynamo_db_table_name" {
  value = aws_dynamodb_table.auth_table.name
}

output "auth_dynamo_db_table_arn" {
  value = aws_dynamodb_table.auth_table.arn
}

# The below outputs are for new DynamoDB table we create for hosting site based auth data.
# We will eventually move to using the name `auth_dynamo_db_table_name` and `auth_dynamo_db_table_arn` once the old tables have been deprecated.
output "authorization_dynamo_db_table_v2_name" {
  value = aws_dynamodb_table.pbs_authorization_table_v2.name
}

output "authorization_dynamo_db_table_v2_arn" {
  value = aws_dynamodb_table.pbs_authorization_table_v2.arn
}