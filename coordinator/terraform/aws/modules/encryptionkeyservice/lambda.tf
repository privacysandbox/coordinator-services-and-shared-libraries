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
  role_name_prefix                               = "${var.environment}_${local.region}_encryption_key_service_lambda"
  get_encryption_key_lambda_alarm_label          = "GetEncryptionKey"
  get_encryption_key_function_name               = "${var.environment}_${local.region}_get_encryption_key_lambda"
  list_recent_encryption_keys_lambda_alarm_label = "ListRecentEncryptionKeys"
  list_recent_encryption_keys_function_name      = "${var.environment}_${local.region}_list_recent_encryption_keys_lambda"
}

module "lambda_roles" {
  source           = "../shared/lambda_roles"
  role_name_prefix = local.role_name_prefix
}

resource "aws_s3_bucket_object" "encryption_key_lambda_package" {
  bucket = var.keymanagement_package_bucket
  key    = "app/encryption_key_lambda.jar"
  source = var.encryption_key_service_jar
  etag   = filemd5(var.encryption_key_service_jar)
}

# Functions are not instrumented with X-Ray.
#tfsec:ignore:aws-lambda-enable-tracing
resource "aws_lambda_function" "get_encryption_key_lambda" {
  function_name = local.get_encryption_key_function_name

  s3_bucket = aws_s3_bucket_object.encryption_key_lambda_package.bucket
  s3_key    = aws_s3_bucket_object.encryption_key_lambda_package.key

  source_code_hash = filebase64sha256(var.encryption_key_service_jar)

  handler = "com.google.scp.coordinator.keymanagement.keyhosting.service.aws.GetEncryptionKeyApiGatewayHandler"
  runtime = "java11"

  timeout     = 10
  memory_size = 1024

  role = module.lambda_roles.trustedparty_lambda_role_arn

  # TODO(b/297923272): Remove DISABLE_ACTIVATION_TIME
  environment {
    variables = {
      KEYSTORE_TABLE_NAME     = var.dynamo_keydb.name
      KEY_DB_REGION           = var.dynamo_keydb.region
      DISABLE_ACTIVATION_TIME = var.get_encryption_key_lambda_ps_client_shim_enabled != "" ? var.get_encryption_key_lambda_ps_client_shim_enabled : "false"
    }
  }

  dynamic "vpc_config" {
    # Add vpc_config block if VPC is enabled. List is a placeholder to use `for_each`.
    for_each = var.enable_vpc ? ["ununsed"] : []
    content {
      security_group_ids = var.lambda_sg_ids
      subnet_ids         = var.private_subnet_ids
    }
  }

  depends_on = [
    module.lambda_roles.trustedparty_lambda_log_attachment,
    # This group is auto-created when creating the lambda, force the log group
    # to be created first so terraform doesn't get a
    # ResourceAlreadyExistsException.
    aws_cloudwatch_log_group.get_encryption_key_lambda_cloudwatch
  ]
}

# Functions are not instrumented with X-Ray.
#tfsec:ignore:aws-lambda-enable-tracing
resource "aws_lambda_function" "list_recent_encryption_keys_lambda" {
  function_name = local.list_recent_encryption_keys_function_name

  s3_bucket = aws_s3_bucket_object.encryption_key_lambda_package.bucket
  s3_key    = aws_s3_bucket_object.encryption_key_lambda_package.key

  source_code_hash = filebase64sha256(var.encryption_key_service_jar)

  handler = "com.google.scp.coordinator.keymanagement.keyhosting.service.aws.ListRecentEncryptionKeysApiGatewayHandler"
  runtime = "java11"

  timeout     = 10
  memory_size = 1024

  role = module.lambda_roles.trustedparty_lambda_role_arn

  environment {
    variables = {
      KEYSTORE_TABLE_NAME = var.dynamo_keydb.name
      KEY_DB_REGION       = var.dynamo_keydb.region
    }
  }

  dynamic "vpc_config" {
    # Add vpc_config block if VPC is enabled. List is a placeholder to use `for_each`.
    for_each = var.enable_vpc ? ["ununsed"] : []
    content {
      security_group_ids = var.lambda_sg_ids
      subnet_ids         = var.private_subnet_ids
    }
  }

  depends_on = [
    module.lambda_roles.trustedparty_lambda_log_attachment,
    # This group is auto-created when creating the lambda, force the log group
    # to be created first so terraform doesn't get a
    # ResourceAlreadyExistsException.
    aws_cloudwatch_log_group.list_recent_encryption_keys_lambda_cloudwatch
  ]
}

# Define permissions for this specific lambda
resource "aws_iam_role_policy" "get_encryption_key_policy" {
  name = "${local.role_name_prefix}_role_policy"
  role = module.lambda_roles.trustedparty_lambda_role_id
  # If Lambda VPC is enabled, then the original policy is superseded by the
  # VPCe policies
  policy = var.enable_vpc ? jsonencode({
    Version = "2012-10-17",
    Statement = [
      {
        Sid      = "DenyAccessFromAnyOtherEndpoints",
        Effect   = "Deny",
        Action   = "dynamodb:*",
        Resource = var.dynamo_keydb.arn,
        Condition = {
          StringNotEquals = {
            "aws:sourceVpce" = var.dynamodb_vpc_endpoint_id
          }
        }
      },
      {
        Sid    = "AllowAccessFromSpecificEndpoint",
        Effect = "Allow",
        Action = [
          "dynamodb:GetItem",
          "dynamodb:Scan",
        ],
        Resource = var.dynamo_keydb.arn
        Condition = {
          StringEquals = {
            "aws:sourceVpce" = var.dynamodb_vpc_endpoint_id
          }
        }
      }
    ]
    }) : jsonencode({
    Version = "2012-10-17",
    Statement = [
      {
        Action = [
          "dynamodb:GetItem",
          "dynamodb:Scan",
        ],
        Resource = var.dynamo_keydb.arn
        Effect   = "Allow"
      }
    ]
  })
}

# Set retention policy for cloudwatch logs.
resource "aws_cloudwatch_log_group" "get_encryption_key_lambda_cloudwatch" {
  name              = "/aws/lambda/${local.get_encryption_key_function_name}"
  retention_in_days = var.logging_retention_days
}

resource "aws_cloudwatch_log_group" "list_recent_encryption_keys_lambda_cloudwatch" {
  name              = "/aws/lambda/${local.list_recent_encryption_keys_function_name}"
  retention_in_days = var.logging_retention_days
}
