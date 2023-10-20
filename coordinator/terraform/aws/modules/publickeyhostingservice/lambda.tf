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

locals {
  role_name_prefix                  = "${var.environment}_${local.region}_get_public_key_lambda"
  function_name                     = "${var.environment}_${local.region}_get_public_key_lambda"
  get_public_key_lambda_alert_label = "GetPublicKey"
}

module "lambda_roles" {
  source           = "../shared/lambda_roles"
  role_name_prefix = local.role_name_prefix
}

resource "aws_s3_bucket_object" "get_public_key_lambda_package" {
  bucket = var.keymanagement_package_bucket
  key    = "app/get_public_key_lambda.jar"
  source = var.public_key_service_jar
  etag   = filemd5(var.public_key_service_jar)
}

# Define lambda configurations
# Functions are not instrumented with X-Ray.
#tfsec:ignore:aws-lambda-enable-tracing
resource "aws_lambda_function" "get_public_key_lambda" {
  function_name = local.function_name

  s3_bucket = var.keymanagement_package_bucket
  s3_key    = aws_s3_bucket_object.get_public_key_lambda_package.key

  source_code_hash = filebase64sha256(var.public_key_service_jar)

  handler     = "com.google.scp.coordinator.keymanagement.keyhosting.service.aws.PublicKeyApiGatewayHandler"
  runtime     = "java11"
  timeout     = 10
  memory_size = var.get_public_key_lambda_memory_mb

  role = module.lambda_roles.trustedparty_lambda_role_arn

  depends_on = [
    module.lambda_roles.trustedparty_lambda_log_attachment,
    aws_cloudwatch_log_group.get_public_key_lambda_cloudwatch,
  ]

  environment {
    variables = {
      KEYSTORE_TABLE_NAME   = var.dynamo_keydb.table_name
      KEY_DB_REGION         = var.dynamo_keydb.region
      CACHE_CONTROL_MAXIMUM = var.get_public_key_cache_control_max_seconds
    }
  }

  dynamic "vpc_config" {
    # Add vpc_config block if VPC is enabled. List is a placeholder to use `for_each`.
    for_each = var.enable_vpc ? ["ununsed"] : []
    content {
      security_group_ids = var.coordinator_vpc_security_groups_ids
      subnet_ids         = var.coordinator_vpc_subnet_ids
    }
  }

  publish = true
}

# Define permissions for this specific lambda
resource "aws_iam_role_policy" "get_public_key_policy" {
  name = "${local.role_name_prefix}_role_policy"
  role = module.lambda_roles.trustedparty_lambda_role_id
  policy = var.enable_vpc ? jsonencode({
    "Version" : "2012-10-17",
    "Statement" : [
      {
        "Sid" : "DenyAccessFromAnyOtherEndpoints",
        "Effect" : "Deny",
        "Action" : "dynamodb:*",
        "Resource" : var.dynamo_keydb.arn,
        "Condition" : {
          "StringNotEquals" : {
            "aws:sourceVpce" : var.keydb_vpc_endpoint_id
          }
        }
      },
      {
        "Sid" : "AllowAccessFromSpecificEndpoint",
        Effect : "Allow"
        Action : [
          "dynamodb:Scan",
        ],
        Resource : var.dynamo_keydb.arn
        "Condition" : {
          "StringEquals" : {
            "aws:sourceVpce" : var.keydb_vpc_endpoint_id
          }
        }
      }
    ]
    }) : jsonencode({
    "Version" : "2012-10-17",
    "Statement" : [
      {
        Action : [
          "dynamodb:Scan"
        ],
        Resource : var.dynamo_keydb.arn
        Effect : "Allow"
      }
    ]
  })
}

# Cloudwatch permissions
resource "aws_cloudwatch_log_group" "get_public_key_lambda_cloudwatch" {
  name              = "/aws/lambda/${local.function_name}"
  retention_in_days = var.get_public_key_logging_retention_days
}

# Provisioned concurrency
resource "aws_lambda_provisioned_concurrency_config" "get_public_key_lambda_provisioned_concurrency" {
  count                             = var.get_public_key_lambda_provisioned_concurrency_enabled ? 1 : 0
  function_name                     = aws_lambda_function.get_public_key_lambda.function_name
  provisioned_concurrent_executions = var.get_public_key_lambda_provisioned_concurrency_count
  qualifier                         = aws_lambda_function.get_public_key_lambda.version
}

# Needed to actually route to version using provisioned concurrency
data "aws_lambda_function" "get_public_key_lambda_versioned_data_source" {
  count         = var.get_public_key_lambda_provisioned_concurrency_enabled ? 1 : 0
  function_name = aws_lambda_function.get_public_key_lambda.function_name
  qualifier     = aws_lambda_function.get_public_key_lambda.version
}

# Policy attachment to Lambda role to access VPC execution; necessary for a Lambda
# to attach a custom VPC. See https://docs.aws.amazon.com/lambda/latest/dg/configuration-vpc.html#vpc-permissions
resource "aws_iam_role_policy_attachment" "vpc_access_execution_role_attachment" {
  count = var.enable_vpc ? 1 : 0

  role       = module.lambda_roles.trustedparty_lambda_role_id
  policy_arn = "arn:aws:iam::aws:policy/service-role/AWSLambdaVPCAccessExecutionRole"
}
