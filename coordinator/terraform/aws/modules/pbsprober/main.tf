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
  role_name_prefix = "${var.environment}_${var.region}_pbsprober_lambda"
  function_name    = "${var.environment}_pbsprober_lambda"
}

module "lambda_roles" {
  source           = "../shared/lambda_roles"
  role_name_prefix = local.role_name_prefix
}

resource "aws_s3_bucket" "pbsprober_bucket" {
  bucket_prefix = "${var.environment}-pbsprober-src-"
  force_destroy = true
}

resource "aws_s3_bucket_object" "pbsprober_bucket_package" {
  bucket = aws_s3_bucket.pbsprober_bucket.id
  key    = "app/pbsprober_lambda.jar"
  source = var.pbsprober_jar
  etag   = filemd5(var.pbsprober_jar)
}

resource "aws_lambda_function" "pbsprober_lambda" {
  function_name    = local.function_name
  s3_bucket        = aws_s3_bucket_object.pbsprober_bucket_package.bucket
  s3_key           = aws_s3_bucket_object.pbsprober_bucket_package.key
  source_code_hash = filebase64sha256(var.pbsprober_jar)
  handler          = "com.google.scp.coordinator.pbsprober.service.aws.PrivacyBudgetProberHandler::handleRequest"
  runtime          = "java11"
  timeout          = 30
  role             = module.lambda_roles.trustedparty_lambda_role_arn
  memory_size      = 512

  environment {
    variables = {
      coordinator_a_privacy_budgeting_endpoint           = var.coordinator_a_privacy_budgeting_endpoint
      coordinator_a_privacy_budget_service_auth_endpoint = var.coordinator_a_privacy_budget_service_auth_endpoint
      coordinator_a_role_arn                             = var.coordinator_a_role_arn
      coordinator_a_region                               = var.coordinator_a_region
      coordinator_b_privacy_budgeting_endpoint           = var.coordinator_b_privacy_budgeting_endpoint
      coordinator_b_privacy_budget_service_auth_endpoint = var.coordinator_b_privacy_budget_service_auth_endpoint
      coordinator_b_role_arn                             = var.coordinator_b_role_arn
      coordinator_b_region                               = var.coordinator_b_region
      reporting_origin                                   = var.reporting_origin
    }
  }
  depends_on = [
    module.lambda_roles.trustedparty_lambda_log_attachment,
  ]
}

# Define permissions for this specific lambda
resource "aws_iam_role_policy" "pbsprober_policy" {
  name = "${local.function_name}_role_policy"
  role = module.lambda_roles.trustedparty_lambda_role_id
  policy = jsonencode({
    "Version" : "2012-10-17",
    "Statement" : [
      {
        Action : [
          "sts:AssumeRole",
        ],
        Resource : "*",
        Effect : "Allow"
      }
    ]
  })
}
