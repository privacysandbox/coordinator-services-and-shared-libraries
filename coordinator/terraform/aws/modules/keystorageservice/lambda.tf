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
  create_key_lambda_alarm_label   = "CreateKey"
  create_key_role_name_prefix     = "${var.environment}_${local.region}_create_key_lambda"
  create_key_function_name        = "${var.environment}_${local.region}_create_key_lambda"
  get_data_key_lambda_alarm_label = "GetDataKey"
  get_data_key_role_name_prefix   = "${var.environment}_${local.region}_get_data_key_lambda"
  get_data_key_function_name      = "${var.environment}_${local.region}_get_data_key_lambda"
}

module "lambda_roles" {
  source           = "../shared/lambda_roles"
  role_name_prefix = local.create_key_role_name_prefix
}

module "worker_enclave_encryption_key" {
  source      = "../shared/workerenclaveencryptionkey"
  environment = "tp2-${var.environment}"
}

resource "aws_s3_bucket_object" "key_storage_package" {
  bucket = var.keymanagement_package_bucket
  key    = "app/key_storage_lambda.jar"
  source = var.key_storage_jar
  etag   = filemd5(var.key_storage_jar)
}

# Define lambda configurations
# Functions are not instrumented with X-Ray.
#tfsec:ignore:aws-lambda-enable-tracing
resource "aws_lambda_function" "create_key_lambda" {
  function_name = local.create_key_function_name

  s3_bucket = aws_s3_bucket_object.key_storage_package.bucket
  s3_key    = aws_s3_bucket_object.key_storage_package.key

  source_code_hash = filebase64sha256(var.key_storage_jar)

  handler     = "com.google.scp.coordinator.keymanagement.keystorage.service.aws.CreateKeyApiGatewayHandler::handleRequest"
  runtime     = "java11"
  timeout     = 10
  memory_size = var.create_key_lambda_memory_mb

  role = module.lambda_roles.trustedparty_lambda_role_arn

  depends_on = [
    module.lambda_roles.trustedparty_lambda_log_attachment,
    # This group is auto-created when creating the lambda, force the log group
    # to be created first so terraform doesn't get a
    # ResourceAlreadyExistsException.
    aws_cloudwatch_log_group.create_key_lambda_cloudwatch
  ]

  environment {
    variables = {
      KEYSTORE_TABLE_NAME                = var.dynamo_keydb.table_name
      AWS_KMS_URI                        = "aws-kms://${module.worker_enclave_encryption_key.key_arn}"
      DATA_KEY_SIGNATURE_KEY_ID          = "${aws_kms_key.data_key_signature_key.arn}"
      DATA_KEY_SIGNATURE_ALGORITHM       = local.signature_key_algorithm
      ENCRYPTION_KEY_SIGNATURE_KEY_ID    = var.enable_public_key_signature ? aws_kms_key.encryption_key_signature_key[0].arn : ""
      ENCRYPTION_KEY_SIGNATURE_ALGORITHM = local.signature_key_algorithm
      # Unused but used by key storage service module
      COORDINATOR_KEK_URI = "aws-kms://${aws_kms_key.data_key_encryptor.arn}"
    }
  }

  publish = true

  dynamic "vpc_config" {
    # Add vpc_config block if VPC is enabled. List is a placeholder to use `for_each`.
    for_each = var.enable_vpc ? ["ununsed"] : []
    content {
      security_group_ids = var.lambda_sg_ids
      subnet_ids         = var.private_subnet_ids
    }
  }
}

# Functions are not instrumented with X-Ray.
#tfsec:ignore:aws-lambda-enable-tracing
resource "aws_lambda_function" "get_data_key_lambda" {
  function_name = local.get_data_key_function_name

  s3_bucket = aws_s3_bucket_object.key_storage_package.bucket
  s3_key    = aws_s3_bucket_object.key_storage_package.key

  source_code_hash = filebase64sha256(var.key_storage_jar)

  handler     = "com.google.scp.coordinator.keymanagement.keystorage.service.aws.GetDataKeyApiGatewayHandler::handleRequest"
  runtime     = "java11"
  timeout     = 10
  memory_size = var.get_data_key_lambda_memory_mb

  role = module.lambda_roles.trustedparty_lambda_role_arn

  depends_on = [
    module.lambda_roles.trustedparty_lambda_log_attachment,
    # This group is auto-created when creating the lambda, force the log group
    # to be created first so terraform doesn't get a
    # ResourceAlreadyExistsException.
    aws_cloudwatch_log_group.get_data_key_lambda_cloudwatch
  ]

  environment {
    variables = {
      KEYSTORE_TABLE_NAME          = var.dynamo_keydb.table_name
      AWS_KMS_URI                  = "aws-kms://${module.worker_enclave_encryption_key.key_arn}"
      COORDINATOR_KEK_URI          = "aws-kms://${aws_kms_key.data_key_encryptor.arn}"
      DATA_KEY_SIGNATURE_KEY_ID    = "${aws_kms_key.data_key_signature_key.arn}"
      DATA_KEY_SIGNATURE_ALGORITHM = local.signature_key_algorithm
    }
  }

  publish = true

  dynamic "vpc_config" {
    # Add vpc_config block if VPC is enabled. List is a placeholder to use `for_each`.
    for_each = var.enable_vpc ? ["ununsed"] : []
    content {
      security_group_ids = var.lambda_sg_ids
      subnet_ids         = var.private_subnet_ids
    }
  }
}

# Define permissions for this specific lambda
resource "aws_iam_role_policy" "create_key_policy" {
  name = "${local.create_key_role_name_prefix}_role_policy"
  role = module.lambda_roles.trustedparty_lambda_role_id
  policy = var.enable_vpc ? jsonencode({
    Version = "2012-10-17",
    Statement = concat([
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
          "dynamodb:PutItem",
        ],
        Resource = var.dynamo_keydb.arn
        Condition = {
          StringEquals = {
            "aws:sourceVpce" = var.dynamodb_vpc_endpoint_id
          }
        }
      },
      {
        Sid      = "RestrictKmsAccessOnlyToVpcEndpoint",
        Effect   = "Deny",
        Action   = "kms:*",
        Resource = module.worker_enclave_encryption_key.key_arn
        Condition = {
          StringNotEquals = {
            "aws:sourceVpce" = var.kms_vpc_endpoint_id
          }
        }
      },
      {
        Sid    = "AllowKmsAccessForKeyCreation",
        Effect = "Allow",
        Action = [
          "kms:Encrypt",
        ],
        Resource = module.worker_enclave_encryption_key.key_arn
        Condition = {
          StringEquals = {
            "aws:sourceVpce" = var.kms_vpc_endpoint_id
          }
        }
      }
      ],
      var.enable_public_key_signature ?
      [{
        Sid    = "AllowKmsAccessForEncryptionKeySigning",
        Effect = "Allow",
        Action = [
          "kms:Sign",
        ],
        Resource = aws_kms_key.encryption_key_signature_key[0].arn
        Condition = {
          StringEquals = {
            "aws:sourceVpce" = var.kms_vpc_endpoint_id
          }
        }
      }]
      : []
    )
    }) : jsonencode({
    Version = "2012-10-17",
    Statement = concat([
      {
        Action = [
          "dynamodb:PutItem",
        ],
        Resource = var.dynamo_keydb.arn
        Effect   = "Allow"
      },
      {
        Action = [
          "kms:Encrypt",
        ],
        Resource = module.worker_enclave_encryption_key.key_arn
        Effect   = "Allow"
      }
      ],
      var.enable_public_key_signature ?
      [{
        Action = [
          "kms:Sign",
        ],
        Resource = aws_kms_key.encryption_key_signature_key[0].arn
        Effect   = "Allow"
      }]
      : []
    )
  })
}

# Define permissions for this specific lambda
resource "aws_iam_role_policy" "get_data_key_policy" {
  name = "${local.get_data_key_role_name_prefix}_role_policy"
  role = module.lambda_roles.trustedparty_lambda_role_id
  policy = var.enable_vpc ? jsonencode({
    Version = "2012-10-17",
    Statement = [
      {
        Sid      = "RestrictKmsSignatureAccessOnlyToVpcEndpoint",
        Effect   = "Deny",
        Action   = "kms:*",
        Resource = aws_kms_key.data_key_signature_key.arn
        Condition = {
          StringNotEquals = {
            "aws:sourceVpce" = var.kms_vpc_endpoint_id
          }
        }
      },
      {
        Sid    = "AllowKmsSignatureAccessForKeyGeneration",
        Effect = "Allow",
        Action = [
          "kms:Sign",
          "kms:Verify",
        ],
        Resource = aws_kms_key.data_key_signature_key.arn
        Condition = {
          StringEquals = {
            "aws:sourceVpce" = var.kms_vpc_endpoint_id
          }
        }
      },
      {
        Sid      = "RestrictEncryptorKmsAccessOnlyToVpcEndpoint",
        Effect   = "Deny",
        Action   = "kms:*",
        Resource = aws_kms_key.data_key_encryptor.arn
        Condition = {
          StringNotEquals = {
            "aws:sourceVpce" = var.kms_vpc_endpoint_id
          }
        }
      },
      {
        Sid    = "AllowEncryptorKmsAccessForKeyEncryption",
        Effect = "Allow",
        Action = [
          "kms:Encrypt",
          "kms:Decrypt",
        ],
        Resource = aws_kms_key.data_key_encryptor.arn
        Condition = {
          StringEquals = {
            "aws:sourceVpce" = var.kms_vpc_endpoint_id
          }
        }
      }
    ]
    }) : jsonencode({
    Version = "2012-10-17",
    Statement = [{
      Action = [
        "kms:Sign",
        "kms:Verify",
      ],
      Resource = aws_kms_key.data_key_signature_key.arn
      Effect   = "Allow"
      },
      {
        Action = [
          "kms:Encrypt",
          "kms:Decrypt",
        ],
        Resource = aws_kms_key.data_key_encryptor.arn
        Effect   = "Allow"
      }
    ]
  })
}

# Set retention policy for cloudwatch logs.
resource "aws_cloudwatch_log_group" "create_key_lambda_cloudwatch" {
  name              = "/aws/lambda/${local.create_key_function_name}"
  retention_in_days = var.create_key_logging_retention_days
}

# Set retention policy for cloudwatch logs.
resource "aws_cloudwatch_log_group" "get_data_key_lambda_cloudwatch" {
  name              = "/aws/lambda/${local.get_data_key_function_name}"
  retention_in_days = var.get_data_key_logging_retention_days
}
