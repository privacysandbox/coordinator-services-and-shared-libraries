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

output "pbs_authorization_table_v2_auth_records" {
  description = "Map of assumed role ARNs to string set of adtech sites. Key = IAM role ARN. Value = String set of adtech sites"
  value = tomap(
    {
      for name, content in aws_dynamodb_table_item.pbs_authorization_table_v2_item :
      jsondecode(content.item)["adtech_identity"]["S"] =>
      jsondecode(content.item)["adtech_sites"]["SS"]
  })
}
