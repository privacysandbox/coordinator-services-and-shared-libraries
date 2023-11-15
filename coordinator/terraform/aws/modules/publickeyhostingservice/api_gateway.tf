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
  api_gateway_alarm_label = "GetPublicKey"
  api_gateway_name        = "${var.environment}_${local.region}_get_public_key_api"
}

resource "aws_apigatewayv2_api" "get_public_key_api_gateway" {
  name          = local.api_gateway_name
  description   = "Get Public Key API"
  protocol_type = "HTTP"
}

# Define route
resource "aws_apigatewayv2_route" "get_public_key_api_gateway_route" {
  api_id    = aws_apigatewayv2_api.get_public_key_api_gateway.id
  route_key = "GET /${var.api_version}/publicKeys"
  target    = "integrations/${aws_apigatewayv2_integration.get_public_key_api_gateway_integration.id}"
}

resource "aws_apigatewayv2_route" "get_public_key_api_gateway_route_well-known" {
  api_id    = aws_apigatewayv2_api.get_public_key_api_gateway.id
  route_key = "GET /.well-known/${var.application_name}/v1/public-keys"
  target    = "integrations/${aws_apigatewayv2_integration.get_public_key_api_gateway_integration.id}"
}

# Define Lambda integration that API Gateway will use
resource "aws_apigatewayv2_integration" "get_public_key_api_gateway_integration" {
  api_id             = aws_apigatewayv2_api.get_public_key_api_gateway.id
  integration_type   = "AWS_PROXY"
  integration_method = "POST"
  integration_uri = (var.get_public_key_lambda_provisioned_concurrency_enabled
    ? data.aws_lambda_function.get_public_key_lambda_versioned_data_source[0].invoke_arn
  : aws_lambda_function.get_public_key_lambda.invoke_arn)

}

# Required permissions for API Gateway to invoke lambda from any resource
resource "aws_lambda_permission" "get_public_key_api_gateway_lambda_permission" {
  statement_id  = "AllowPublicKeyAPIGatewayInvoke"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.get_public_key_lambda.function_name
  principal     = "apigateway.amazonaws.com"
  source_arn    = "${aws_apigatewayv2_api.get_public_key_api_gateway.execution_arn}/*/*"
  qualifier     = var.get_public_key_lambda_provisioned_concurrency_enabled ? aws_lambda_function.get_public_key_lambda.version : null
}

resource "aws_apigatewayv2_stage" "get_public_key_api_gateway_stage" {
  api_id      = aws_apigatewayv2_api.get_public_key_api_gateway.id
  name        = var.api_env_stage_name
  auto_deploy = true

  lifecycle {
    create_before_destroy = true
  }

  default_route_settings {
    detailed_metrics_enabled = true
    logging_level            = "INFO"
    # Will be 0 if not defined. These are the max values
    throttling_burst_limit = 5000
    throttling_rate_limit  = 10000
  }

  access_log_settings {
    destination_arn = aws_cloudwatch_log_group.get_public_key_api_gateway_log_group.arn
    format = jsonencode({
      requestId       = "$context.requestId",
      requestTime     = "$context.requestTime",
      httpMethod      = "$context.httpMethod",
      routeKey        = "$context.routeKey",
      status          = "$context.status",
      responseLatency = "$context.responseLatency",
      responseLength  = "$context.responseLength",
      userArn         = "$context.identity.userArn"
    })
  }

  depends_on = [aws_cloudwatch_log_group.get_public_key_api_gateway_log_group]
}

# Use vended logs log group to store logs so resource policy does not run into size limits.
# https://docs.aws.amazon.com/AmazonCloudWatch/latest/logs/AWS-logs-and-resource-policy.html
resource "aws_cloudwatch_log_group" "get_public_key_api_gateway_log_group" {
  name              = "/aws/vendedlogs/apigateway/${local.api_gateway_name}"
  retention_in_days = var.get_public_key_logging_retention_days
}
