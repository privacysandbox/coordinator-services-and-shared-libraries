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

#################################################################################
# HTTP Api Gateway
################################################################################

locals {
  create_key_route   = "${var.api_version}/encryptionKeys"
  get_data_key_route = "${var.api_version}/encryptionKeys:getDataKey"
}

resource "aws_apigatewayv2_route" "create_key_api_gateway_route" {
  api_id             = var.api_gateway_id
  route_key          = "POST /${local.create_key_route}"
  target             = "integrations/${aws_apigatewayv2_integration.create_key_api_gateway_integration.id}"
  authorization_type = "AWS_IAM"
}

resource "aws_apigatewayv2_integration" "create_key_api_gateway_integration" {
  api_id             = var.api_gateway_id
  integration_type   = "AWS_PROXY"
  integration_method = "POST"
  integration_uri    = aws_lambda_function.create_key_lambda.invoke_arn
}

resource "aws_lambda_permission" "create_key_api_gateway_permission" {
  statement_id  = "AllowCreateKeyExecutionFromGateway"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.create_key_lambda.function_name
  principal     = "apigateway.amazonaws.com"
  source_arn    = "${var.api_gateway_execution_arn}/*/*/${var.api_version}/encryptionKeys"
}

resource "aws_apigatewayv2_route" "get_data_key_api_gateway_route" {
  api_id             = var.api_gateway_id
  route_key          = "POST /${local.get_data_key_route}"
  target             = "integrations/${aws_apigatewayv2_integration.get_data_key_api_gateway_integration.id}"
  authorization_type = "AWS_IAM"
}

resource "aws_apigatewayv2_integration" "get_data_key_api_gateway_integration" {
  api_id             = var.api_gateway_id
  integration_type   = "AWS_PROXY"
  integration_method = "POST"
  integration_uri    = aws_lambda_function.get_data_key_lambda.invoke_arn
}

resource "aws_lambda_permission" "get_data_key_api_gateway_permission" {
  statement_id  = "AllowCreateKeyExecutionFromGateway"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.get_data_key_lambda.function_name
  principal     = "apigateway.amazonaws.com"
  source_arn    = "${var.api_gateway_execution_arn}/*/*/${var.api_version}/encryptionKeys:getDataKey"
}
