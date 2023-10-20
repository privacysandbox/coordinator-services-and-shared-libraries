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

data "aws_caller_identity" "current" {}

locals {
  account_id = data.aws_caller_identity.current.account_id
}

resource "aws_s3_object" "onboarding_zip" {
  bucket = var.adtech_mgmt_storage_bucket_name
  key    = "onboarding.zip"
  source = var.handle_onboarding_request_lambda_file_path
  etag   = filemd5(var.handle_onboarding_request_lambda_file_path)
}

resource "aws_lambda_function" "handle_onboarding_request" {
  function_name    = "${var.environment}-handle-onboarding-request"
  s3_bucket        = aws_s3_object.onboarding_zip.bucket
  s3_key           = aws_s3_object.onboarding_zip.key
  source_code_hash = filebase64sha256(var.handle_onboarding_request_lambda_file_path)
  handler          = "onboard.handler"
  runtime          = "nodejs18.x"
  role             = aws_iam_role.handle_onboarding_lambda.arn
  environment {
    variables = {
      ADTECH_MGMT_DB_TABLE_NAME = var.adtech_mgmt_db_name
      PBS_AUTH_DB_TABLE_NAME    = var.pbs_auth_db_name
      POLICY_ARN                = var.iam_policy_arn
    }
  }
  depends_on = [aws_iam_role.handle_onboarding_lambda, aws_s3_object.onboarding_zip]
}

resource "aws_iam_role" "handle_onboarding_lambda" {
  name = "${var.environment}-handle-onboarding-lambda-role"
  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Action = "sts:AssumeRole"
        Effect = "Allow"
        Sid    = ""
        Principal = {
          Service = "lambda.amazonaws.com"
        }
      },
    ]
  })
}

resource "aws_iam_role_policy" "handle_onboarding_role_policy" {
  name = "${var.environment}-handle_onboarding_role_policy"
  role = aws_iam_role.handle_onboarding_lambda.id
  policy = jsonencode({
    "Version" : "2012-10-17",
    "Statement" : [
      {
        "Action" : [
          "iam:AttachRolePolicy",
          "iam:CreateRole",
          "iam:DeleteRole",
          "iam:DetachRolePolicy"
        ],
        "Effect" : "Allow",
        "Resource" : "*"
      },
      {
        "Action" : [
          "dynamodb:BatchGetItem",
          "dynamodb:GetItem",
          "dynamodb:DeleteItem",
          "dynamodb:Query",
          "dynamodb:Scan",
          "dynamodb:BatchWriteItem",
          "dynamodb:PutItem",
          "dynamodb:UpdateItem"
        ],
        "Effect" : "Allow",
        "Resource" : [
          var.adtech_mgmt_db_arn,
          var.pbs_auth_db_arn
        ]
      },
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
        "Resource" : "arn:aws:logs:${var.aws_region}:${data.aws_caller_identity.current.account_id}:log-group:/aws/lambda/${aws_lambda_function.handle_onboarding_request.function_name}:*"
      }
    ]
  })
}

resource "aws_s3_object" "offboarding_zip" {
  bucket = var.adtech_mgmt_storage_bucket_name
  key    = "offboarding.zip"
  source = var.handle_offboarding_request_lambda_file_path
  etag   = filemd5(var.handle_offboarding_request_lambda_file_path)
}

resource "aws_lambda_function" "handle_offboarding_request" {
  function_name    = "${var.environment}-handle-offboarding-request"
  s3_bucket        = aws_s3_object.offboarding_zip.bucket
  s3_key           = aws_s3_object.offboarding_zip.key
  source_code_hash = filebase64sha256(var.handle_offboarding_request_lambda_file_path)
  handler          = "offboard.handler"
  runtime          = "nodejs18.x"
  role             = aws_iam_role.handle_offboarding_lambda.arn
  environment {
    variables = {
      ADTECH_MGMT_DB_TABLE_NAME = var.adtech_mgmt_db_name
      PBS_AUTH_DB_TABLE_NAME    = var.pbs_auth_db_name
      POLICY_ARN                = var.iam_policy_arn
    }
  }
  depends_on = [aws_iam_role.handle_offboarding_lambda, aws_s3_object.offboarding_zip]
}

resource "aws_iam_role" "handle_offboarding_lambda" {
  name = "${var.environment}-handle-offboarding-lambda-role"
  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Action = "sts:AssumeRole"
        Effect = "Allow"
        Sid    = ""
        Principal = {
          Service = "lambda.amazonaws.com"
        }
      },
    ]
  })
}

resource "aws_iam_role_policy" "handle_offboarding_role_policy" {
  name = "${var.environment}-handle-offboarding-role-policy"
  role = aws_iam_role.handle_offboarding_lambda.id
  policy = jsonencode({
    "Version" : "2012-10-17",
    "Statement" : [
      {
        "Action" : [
          "iam:AttachRolePolicy",
          "iam:CreateRole",
          "iam:DeleteRole",
          "iam:DetachRolePolicy"
        ],
        "Effect" : "Allow",
        "Resource" : "*"
      },
      {
        "Action" : [
          "dynamodb:BatchGetItem",
          "dynamodb:GetItem",
          "dynamodb:DeleteItem",
          "dynamodb:Query",
          "dynamodb:Scan",
          "dynamodb:BatchWriteItem",
          "dynamodb:PutItem",
          "dynamodb:UpdateItem"
        ],
        "Effect" : "Allow",
        "Resource" : [
          var.adtech_mgmt_db_arn,
          var.pbs_auth_db_arn
        ]
      },
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
        "Resource" : "arn:aws:logs:${var.aws_region}:${data.aws_caller_identity.current.account_id}:log-group:/aws/lambda/${aws_lambda_function.handle_offboarding_request.function_name}:*"
      }
    ]
  })
}

resource "aws_s3_object" "read_zip" {
  bucket = var.adtech_mgmt_storage_bucket_name
  key    = "read.zip"
  source = var.handle_read_adtech_request_lambda_file_path
  etag   = filemd5(var.handle_read_adtech_request_lambda_file_path)
}

resource "aws_lambda_function" "handle_read_adtech_request" {
  function_name    = "${var.environment}-handle-read-adtech-request"
  s3_bucket        = aws_s3_object.read_zip.bucket
  s3_key           = aws_s3_object.read_zip.key
  source_code_hash = filebase64sha256(var.handle_read_adtech_request_lambda_file_path)
  handler          = "read.handler"
  runtime          = "nodejs18.x"
  role             = aws_iam_role.handle_read_adtech_lambda.arn
  environment {
    variables = {
      ADTECH_MGMT_DB_TABLE_NAME = var.adtech_mgmt_db_name
      PBS_AUTH_DB_TABLE_NAME    = var.pbs_auth_db_name
      POLICY_ARN                = var.iam_policy_arn
    }
  }
  depends_on = [aws_iam_role.handle_read_adtech_lambda, aws_s3_object.read_zip]
}

resource "aws_iam_role" "handle_read_adtech_lambda" {
  name = "${var.environment}-handle-read-adtech-lambda-role"
  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Action = "sts:AssumeRole"
        Effect = "Allow"
        Sid    = ""
        Principal = {
          Service = "lambda.amazonaws.com"
        }
      },
    ]
  })
}

resource "aws_iam_role_policy" "handle_read_adtech_role_policy" {
  name = "${var.environment}-handle-read-adtech-role-policy"
  role = aws_iam_role.handle_read_adtech_lambda.id
  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        "Action" : [
          "dynamodb:BatchGetItem",
          "dynamodb:GetItem",
          "dynamodb:DeleteItem",
          "dynamodb:Query",
          "dynamodb:Scan",
        ],
        Effect   = "Allow"
        Resource = var.adtech_mgmt_db_arn
      },
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
        "Resource" : "arn:aws:logs:${var.aws_region}:${data.aws_caller_identity.current.account_id}:log-group:/aws/lambda/${aws_lambda_function.handle_read_adtech_request.function_name}:*"
      }
    ]
  })
}
