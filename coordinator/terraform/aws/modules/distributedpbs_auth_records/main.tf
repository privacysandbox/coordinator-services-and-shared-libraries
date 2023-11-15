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

terraform {
  required_version = "~> 1.2.3"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 3.0"
    }
  }
}

resource "aws_dynamodb_table_item" "pbs_authorization_table_v2_item" {
  # Create nothing if no table name is given
  for_each   = var.privacy_budget_auth_table_name == "" ? {} : var.allowed_principals_map_v2
  table_name = var.privacy_budget_auth_table_name
  hash_key   = "adtech_identity"
  item       = <<ITEM
{
  "adtech_identity": {"S": "${var.coordinator_assume_role_arns[each.key]}"},
  "adtech_sites": {"SS": ${jsonencode(each.value)}}
}
ITEM
}
