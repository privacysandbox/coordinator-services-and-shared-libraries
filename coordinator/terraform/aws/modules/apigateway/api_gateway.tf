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
  api_gateway_name = "${var.environment}_${local.region}_${var.name}"
}

resource "aws_apigatewayv2_api" "api_gateway" {
  name          = local.api_gateway_name
  protocol_type = "HTTP"
}
resource "aws_apigatewayv2_stage" "api_gateway_stage" {
  api_id      = aws_apigatewayv2_api.api_gateway.id
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
    destination_arn = aws_cloudwatch_log_group.api_gateway_log_group.arn
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

  depends_on = [aws_cloudwatch_log_group.api_gateway_log_group]
}

# Use vended logs log group to store logs so resource policy does not run into size limits.
# https://docs.aws.amazon.com/AmazonCloudWatch/latest/logs/AWS-logs-and-resource-policy.html
# Note: The below tfsec issues are ignored because cloudwatch uses encryption
# by default and there is not sensitive information.
#tfsec:ignore:aws-cloudwatch-log-group-customer-key
resource "aws_cloudwatch_log_group" "api_gateway_log_group" {
  name              = "/aws/vendedlogs/apigateway/${local.api_gateway_name}"
  retention_in_days = var.api_gateway_logging_retention_days
}
