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

resource "aws_cognito_user_pool" "adtech_mgmt_coordinator_api_users" {
  name = "${var.environment}-adtech-mgmt-coordinator-api-users"
}

resource "aws_cognito_user_pool_client" "adtech_mgmt_coordinator_api_cognito_client" {
  name         = "${var.environment}-adtech-mgmt-coord-api-cognito-client"
  user_pool_id = aws_cognito_user_pool.adtech_mgmt_coordinator_api_users.id
  explicit_auth_flows = [
    "ALLOW_USER_PASSWORD_AUTH",
    "ALLOW_USER_SRP_AUTH",
    "ALLOW_REFRESH_TOKEN_AUTH"
  ]
}
