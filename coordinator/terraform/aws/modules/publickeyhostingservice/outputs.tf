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

output "api_gateway_url" {
  value = aws_apigatewayv2_stage.get_public_key_api_gateway_stage.invoke_url
}

output "api_gateway_id" {
  value = aws_apigatewayv2_api.get_public_key_api_gateway.id
}

output "lambda_version" {
  value = aws_lambda_function.get_public_key_lambda.version
}

output "lambda_assumed_role" {
  value = {
    "assumed_role" : replace(
      replace(
        aws_lambda_function.get_public_key_lambda.role,
      "/:role/", ":assumed-role"),
    "/:iam/", ":sts"),
    "function_name" : aws_lambda_function.get_public_key_lambda.function_name,
  }
}

output "get_public_key_lambda_principal_role_arn" {
  value = module.lambda_roles.trustedparty_lambda_role_arn
}

output "get_public_key_lambda_principal_role_id" {
  value = module.lambda_roles.trustedparty_lambda_role_id
}
