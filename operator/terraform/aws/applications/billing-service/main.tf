/**
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

provider "aws" {
  region = "us-east-1"
}

module "bazel" {
  source = "../../modules/bazel"
}

module "get-cost-lambda" {
  source                 = "terraform-aws-modules/lambda/aws"
  memory_size            = 1024
  timeout                = 30
  function_name          = "${var.environment}-get-cost"
  description            = "Gets the cost for the specified cloud resource"
  handler                = "com.google.scp.microservice.cost.aws.handlers.GetPriceHandler"
  runtime                = "java11"
  source_path            = "../../../../../java/com/google/scp/microservice/cost/aws/handlers"
  create_package         = false
  local_existing_package = "${module.bazel.bazel_bin}/java/com/google/scp/microservice/cost/aws/handlers/AwsCostService_deploy.jar"
  tags = {
    Environment = "${var.environment}"
  }

  policy_json = jsonencode({
    "Version" : "2012-10-17",
    "Statement" : [
      {
        "Sid" : "AllowPricing",
        "Effect" : "Allow",
        "Action" : "pricing:*",
        "Resource" : "*"
      }
    ]
  })

  attach_policy_json = true

  publish = true

  allowed_triggers = {
    AllowExecutionFromAPIGateway = {
      service    = "apigateway"
      source_arn = "${module.cost-service-api-gateway.apigatewayv2_api_execution_arn}/*/*"
    }
  }
}

module "cost-service-api-gateway" {
  source = "terraform-aws-modules/apigateway-v2/aws"

  name          = "${var.environment}-cost-service"
  description   = "The api gateway for the Cost Service"
  protocol_type = "HTTP"


  create_api_domain_name = false
  # Routes and integrations
  integrations = {
    "POST /getCost" = {
      lambda_arn             = module.get-cost-lambda.lambda_function_arn
      payload_format_version = "2.0"
      timeout_milliseconds   = 12000
    }

  }

  tags = {
    Environment = "${var.environment}"
  }
}
