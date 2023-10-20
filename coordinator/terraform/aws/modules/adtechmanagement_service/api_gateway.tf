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

locals {
  onboarding_route_name  = "PUT /onboardAdtech"
  offboarding_route_name = "DELETE /offboardAdtech"
  read_adtech_route_name = "POST /readAdtech"
}

resource "aws_apigatewayv2_api" "adtech_mgmt_coordinator_api" {
  name          = "${var.environment}-adtech-mgmt-coordinator-api"
  protocol_type = "HTTP"
  cors_configuration {
    allow_credentials = true
    allow_headers     = ["Authorization", "*"]
    allow_methods     = ["*"]
    allow_origins     = ["http://*", "https://*"]
    max_age           = 300
  }
}

resource "aws_apigatewayv2_authorizer" "adtech_mgmt_coordinator_api_cognito_authorizer" {
  api_id           = aws_apigatewayv2_api.adtech_mgmt_coordinator_api.id
  authorizer_type  = "JWT"
  identity_sources = ["$request.header.Authorization"]
  name             = "cognito-authorizer"

  jwt_configuration {
    audience = [aws_cognito_user_pool_client.adtech_mgmt_coordinator_api_cognito_client.id]
    issuer   = "https://${aws_cognito_user_pool.adtech_mgmt_coordinator_api_users.endpoint}"
  }
}

resource "aws_apigatewayv2_integration" "handle_onboard_adtech_integration" {
  api_id             = aws_apigatewayv2_api.adtech_mgmt_coordinator_api.id
  integration_type   = "AWS_PROXY"
  description        = "Integrating handle-onboard-request lambda with ${local.onboarding_route_name} endpoint."
  integration_method = "POST"
  integration_uri    = aws_lambda_function.handle_onboarding_request.invoke_arn
}

resource "aws_apigatewayv2_integration" "handle_offboard_adtech_integration" {
  api_id             = aws_apigatewayv2_api.adtech_mgmt_coordinator_api.id
  integration_type   = "AWS_PROXY"
  description        = "Integrating handle-offboard-request lambda with ${local.offboarding_route_name} endpoint."
  integration_method = "POST"
  integration_uri    = aws_lambda_function.handle_offboarding_request.invoke_arn
}

resource "aws_apigatewayv2_integration" "handle_read_adtech_adtech_integration" {
  api_id             = aws_apigatewayv2_api.adtech_mgmt_coordinator_api.id
  integration_type   = "AWS_PROXY"
  description        = "Integrating handle-read-adtech-request lambda with ${local.read_adtech_route_name} endpoint."
  integration_method = "POST"
  integration_uri    = aws_lambda_function.handle_read_adtech_request.invoke_arn
}

resource "aws_apigatewayv2_route" "handle_onboarding" {
  api_id             = aws_apigatewayv2_api.adtech_mgmt_coordinator_api.id
  route_key          = local.onboarding_route_name
  authorization_type = "JWT"
  authorizer_id      = aws_apigatewayv2_authorizer.adtech_mgmt_coordinator_api_cognito_authorizer.id
  target             = "integrations/${aws_apigatewayv2_integration.handle_onboard_adtech_integration.id}"
}

resource "aws_apigatewayv2_route" "handle_offboarding" {
  api_id             = aws_apigatewayv2_api.adtech_mgmt_coordinator_api.id
  route_key          = local.offboarding_route_name
  authorization_type = "JWT"
  authorizer_id      = aws_apigatewayv2_authorizer.adtech_mgmt_coordinator_api_cognito_authorizer.id
  target             = "integrations/${aws_apigatewayv2_integration.handle_offboard_adtech_integration.id}"
}

resource "aws_apigatewayv2_route" "handle_read_adtech" {
  api_id             = aws_apigatewayv2_api.adtech_mgmt_coordinator_api.id
  route_key          = local.read_adtech_route_name
  authorizer_id      = aws_apigatewayv2_authorizer.adtech_mgmt_coordinator_api_cognito_authorizer.id
  authorization_type = "JWT"
  target             = "integrations/${aws_apigatewayv2_integration.handle_read_adtech_adtech_integration.id}"
}

resource "aws_lambda_permission" "handle_onboarding_lambda_permission" {
  statement_id  = "AllowHandleOnboardingRequestExecuteFromGateway"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.handle_onboarding_request.function_name
  principal     = "apigateway.amazonaws.com"
  source_arn    = "${aws_apigatewayv2_api.adtech_mgmt_coordinator_api.execution_arn}/*"
}

resource "aws_lambda_permission" "handle_offboarding_lambda_permission" {
  statement_id  = "AllowHandleOffboardingRequestExecuteFromGateway"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.handle_offboarding_request.function_name
  principal     = "apigateway.amazonaws.com"
  source_arn    = "${aws_apigatewayv2_api.adtech_mgmt_coordinator_api.execution_arn}/*"
}

resource "aws_lambda_permission" "handle_read_adtech_lambda_permission" {
  statement_id  = "AllowHandleReadRequestExecuteFromGateway"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.handle_read_adtech_request.function_name
  principal     = "apigateway.amazonaws.com"
  source_arn    = "${aws_apigatewayv2_api.adtech_mgmt_coordinator_api.execution_arn}/*"
}

resource "aws_iam_role" "adtech_mgmt_coordinator_api_gateway" {
  name = "${var.environment}-adtech-mgmt-coordinator-api-gateway-role"
  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Action = "sts:AssumeRole"
        Effect = "Allow"
        Sid    = ""
        Principal = {
          Service = "apigateway.amazonaws.com"
        }
      },
    ]
  })
}

resource "aws_iam_role_policy" "adtech_mgmt_coordinator_api_gateway_policy" {
  name = "${var.environment}-adtech-mgmt-coordinator-api-gateway-policy"
  role = aws_iam_role.adtech_mgmt_coordinator_api_gateway.id
  policy = jsonencode({
    "Version" : "2012-10-17",
    "Statement" : [
      {
        "Effect" : "Allow",
        "Action" : "logs:CreateLogGroup",
        "Resource" : "arn:aws:logs:${var.aws_region}:${data.aws_caller_identity.current.account_id}:*"

      },
      {
        "Action" : [
          "logs:CreateLogStream",
          "logs:PutLogEvents"
        ],
        "Effect" : "Allow",
        "Resource" : "arn:aws:logs:${var.aws_region}:${data.aws_caller_identity.current.account_id}:log-group:${aws_cloudwatch_log_group.adtech_mgmt_coordinator_api_lambda_integration.name}:*"
      }
    ]
  })
}
resource "aws_apigatewayv2_stage" "adtech_mgmt_coordinator_api_stage" {
  api_id      = aws_apigatewayv2_api.adtech_mgmt_coordinator_api.id
  name        = "${var.environment}-adtech-mgmt-coordinator-api-stage"
  description = "creating stage in order to produce invoke url"
  auto_deploy = true
  access_log_settings {
    destination_arn = aws_cloudwatch_log_group.adtech_mgmt_coordinator_api_lambda_integration.arn
    format = jsonencode({
      "requestId" : "$context.requestId",
      "ip" : "$context.identity.sourceIp",
      "requestTime" : "$context.requestTime",
      "httpMethod" : "$context.httpMethod",
      "routeKey" : "$context.routeKey",
      "status" : "$context.status",
      "integrationRequestId" : "$context.integration.requestId",
      "functionResponseStatus" : "$context.integration.status",
      "authorizeResultStatus" : "$context.authorizer.status",
      "authorizerRequestId" : "$context.authorizer.requestId",
      "cognitoUser" : "$context.identity.cognitoIdentityId"
      "user" : "$context.identity.user"
    })
  }
}

resource "aws_cloudwatch_log_group" "adtech_mgmt_coordinator_api_lambda_integration" {
  name              = "/aws/vendedlogs/${var.environment}-adtech-mgmt-coordinator-api-lambda-integration"
  retention_in_days = var.log_retention_in_days
}
