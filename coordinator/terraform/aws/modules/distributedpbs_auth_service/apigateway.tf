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

resource "aws_apigatewayv2_api" "lambda_api" {
  name          = "${var.environment_prefix}-google-scp-pbs-auth-api"
  protocol_type = "HTTP"
  target        = aws_lambda_function.lambda.arn

  depends_on = [
    aws_lambda_function.lambda
  ]
}

resource "aws_apigatewayv2_integration" "post_api_integration" {
  api_id             = aws_apigatewayv2_api.lambda_api.id
  integration_type   = "AWS_PROXY"
  integration_method = "POST"
  integration_uri    = aws_lambda_function.lambda.invoke_arn
}

resource "aws_apigatewayv2_route" "consume_privacy_budget_api_gateway_route" {
  api_id             = aws_apigatewayv2_api.lambda_api.id
  route_key          = "POST /auth"
  target             = "integrations/${aws_apigatewayv2_integration.post_api_integration.id}"
  authorization_type = "AWS_IAM"
}

resource "aws_lambda_permission" "lambda_api_gateway_post" {
  statement_id  = "AllowExecutionFromAPIGatewayPost"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.lambda.function_name
  principal     = "apigateway.amazonaws.com"
  source_arn    = "${aws_apigatewayv2_api.lambda_api.execution_arn}/*/POST/auth"
}

resource "aws_apigatewayv2_stage" "api_gateway_stage" {
  api_id      = aws_apigatewayv2_api.lambda_api.id
  name        = var.environment_prefix
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
    destination_arn = aws_cloudwatch_log_group.api_logs.arn
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

  depends_on = [aws_cloudwatch_log_group.api_logs]
}

# Use vended logs log group to store logs so resource policy does not run into size limits.
# https://docs.aws.amazon.com/AmazonCloudWatch/latest/logs/AWS-logs-and-resource-policy.html
resource "aws_cloudwatch_log_group" "api_logs" {
  name              = "/aws/vendedlogs/apigateway/${aws_apigatewayv2_api.lambda_api.name}"
  retention_in_days = 90
}
