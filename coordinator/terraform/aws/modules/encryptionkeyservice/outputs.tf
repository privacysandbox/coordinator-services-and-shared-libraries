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

output "lambda_principal_role_arn" {
  value = module.lambda_roles.trustedparty_lambda_role_arn
}

output "lambda_execution_role_id" {
  value = module.lambda_roles.trustedparty_lambda_role_id
}

output "lambda_version" {
  value = aws_lambda_function.get_encryption_key_lambda.version
}
