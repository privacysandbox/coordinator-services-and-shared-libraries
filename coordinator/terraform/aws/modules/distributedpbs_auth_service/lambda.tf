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
  role_name_prefix       = "${var.environment_prefix}-google-scp-pbs-auth-lambda"
  pbs_auth_function_name = "${var.environment_prefix}-google-scp-pbs-auth-lambda"
}

module "lambda_roles" {
  source           = "../shared/lambda_roles"
  role_name_prefix = local.role_name_prefix
}

data "aws_iam_policy_document" "dynamo_db_access_policy_document" {
  statement {
    actions   = ["dynamodb:GetItem"]
    resources = ["${var.pbs_authorization_v2_table_arn}"]
  }
}

resource "aws_iam_role_policy" "dynamo_db_access_policy" {
  name = "${local.pbs_auth_function_name}-dynamo-policy"
  role = module.lambda_roles.trustedparty_lambda_role_id

  policy = data.aws_iam_policy_document.dynamo_db_access_policy_document.json

  depends_on = [
    module.lambda_roles.trustedparty_lambda_log_attachment,
  ]
}

resource "aws_lambda_function" "lambda" {
  filename         = var.auth_lambda_handler_path
  function_name    = local.pbs_auth_function_name
  role             = module.lambda_roles.trustedparty_lambda_role_arn
  handler          = "auth_lambda_handler.lambda_handler"
  source_code_hash = filebase64sha256("${var.auth_lambda_handler_path}")
  runtime          = "python3.9"

  environment {
    variables = {
      PBS_AUTHORIZATION_V2_DYNAMODB_TABLE_NAME = "${var.pbs_authorization_v2_table_name}"
    }
  }

  depends_on = [
    module.lambda_roles.trustedparty_lambda_log_attachment,
    # This group is auto-created when creating the lambda, force the log group
    # to be created first so terraform doesn't get a
    # ResourceAlreadyExistsException.
    aws_cloudwatch_log_group.lambda_cloudwatch
  ]
}

resource "aws_cloudwatch_log_group" "lambda_cloudwatch" {
  name              = "/aws/lambda/${local.pbs_auth_function_name}"
  retention_in_days = var.logging_retention_days
}