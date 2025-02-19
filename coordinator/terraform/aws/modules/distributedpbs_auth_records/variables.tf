# Copyright 2025 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

variable "privacy_budget_auth_table_name" {
  description = "DynamoDB table name of distributed pbs auth table"
  type        = string
}

variable "allowed_principals_map_v2" {
  type        = map(list(string))
  description = "Map of AWS account IDs that will the assume coordinator_assume_role associated with their list of sites."
}

variable "coordinator_assume_role_arns" {
  type        = map(string)
  description = "Map of Account ID to corresponding assumed role ARNs. Key = Account ID. Value = assumed role ARN."
}
