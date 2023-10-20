# Copyright 2023 Google LLC
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

resource "aws_dynamodb_table" "adtech_management_db" {
  name         = "${var.environment}-adtech-management"
  billing_mode = "PAY_PER_REQUEST"
  hash_key     = "iam_role"
  point_in_time_recovery {
    enabled = true
  }
  attribute {
    name = "iam_role"
    type = "S"
  }
  attribute {
    name = "adtech_id"
    type = "S"
  }
  global_secondary_index {
    name               = "AdtechIdIndex"
    hash_key           = "adtech_id"
    projection_type    = "INCLUDE"
    non_key_attributes = ["origin_url", "aws_id"]
  }
}

output "arn" {
  value = aws_dynamodb_table.adtech_management_db.arn
}

output "name" {
  value = aws_dynamodb_table.adtech_management_db.name
}
