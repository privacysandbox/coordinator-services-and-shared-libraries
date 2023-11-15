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

data "aws_iam_policy_document" "lambda_iam_role_policy_document" {
  statement {
    actions = ["sts:AssumeRole"]
    principals {
      type        = "Service"
      identifiers = ["lambda.amazonaws.com"]
    }
  }
}

resource "aws_iam_role" "lambda_iam_role" {
  name = "${var.environment_prefix}-google-scp-pbs-auth-lambda-iam-role"

  assume_role_policy = data.aws_iam_policy_document.lambda_iam_role_policy_document.json
}

data "aws_iam_policy_document" "dynamo_db_access_policy_document" {
  statement {
    actions   = ["dynamodb:GetItem"]
    resources = ["${var.auth_dynamo_db_table_arn}", "${var.pbs_authorization_v2_table_arn}"]
  }
}

resource "aws_iam_role_policy" "dynamo_db_access_policy" {
  name = "${var.environment_prefix}-google-scp-pbs-auth-lambda-dynamo-policy"
  role = aws_iam_role.lambda_iam_role.id

  policy = data.aws_iam_policy_document.dynamo_db_access_policy_document.json

  depends_on = [
    aws_iam_role.lambda_iam_role
  ]
}

resource "aws_iam_role_policy_attachment" "lambda_policy" {
  role       = aws_iam_role.lambda_iam_role.name
  policy_arn = "arn:aws:iam::aws:policy/service-role/AWSLambdaBasicExecutionRole"

  depends_on = [
    aws_iam_role.lambda_iam_role
  ]
}

resource "aws_lambda_function" "lambda" {
  filename         = var.auth_lambda_handler_path
  function_name    = "${var.environment_prefix}-google-scp-pbs-auth-lambda"
  role             = aws_iam_role.lambda_iam_role.arn
  handler          = "auth_lambda_handler.lambda_handler"
  source_code_hash = filebase64sha256("${var.auth_lambda_handler_path}")
  runtime          = "python3.8"

  environment {
    variables = {
      REPORTING_ORIGIN_AUTH_DYNAMO_DB_TABLE_NAME = "${var.auth_dynamo_db_table_name}",
      PBS_AUTHORIZATION_V2_DYNAMODB_TABLE_NAME   = "${var.pbs_authorization_v2_table_name}"
    }
  }
}
