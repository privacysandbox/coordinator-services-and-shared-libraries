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

output "pbs_environment_url" {
  value = "DEPRECATED"
}

output "pbs_environment_auth_url" {
  value = "${module.auth_service.api_gateway_base_url}/auth"
}

output "pbs_auth_api_gateway_arn" {
  value = module.auth_service.api_gateway_arn
}

output "pbs_auth_table_v2_name" {
  value = module.auth_db.authorization_dynamo_db_table_v2_name
}
