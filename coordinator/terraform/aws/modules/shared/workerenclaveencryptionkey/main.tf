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

terraform {
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 3.0"
    }
  }
}

data "aws_region" "current" {}

locals {
  region       = data.aws_region.current.name
  key_env_name = var.environment_prefix != "" ? "${var.environment_prefix}-${var.environment}" : "${var.environment}"
}

resource "aws_kms_key" "private_key_encryptor" {
  description         = "${local.key_env_name}_${local.region}_worker_key"
  enable_key_rotation = true
}

resource "aws_kms_alias" "private_key_encryptor_alias" {
  name          = "alias/${local.key_env_name}-${local.region}-worker-kms-key"
  target_key_id = aws_kms_key.private_key_encryptor.key_id
}

resource "aws_iam_policy" "kek_key_sync_policy" {
  count = var.key_sync_service_account_unique_id != "" ? 1 : 0
  name  = "${var.environment}-kek-key-sync-policy"
  path  = "/"

  policy = jsonencode({
    Version = "2012-10-17",
    Statement = [
      {
        "Action" : [
          "kms:Encrypt"
        ]
        "Resource" : [
          aws_kms_key.private_key_encryptor.arn
        ]
        "Effect" : "Allow"
      }
    ]
  })
}

resource "aws_iam_role_policy_attachment" "kek_key_sync_policy_attachment" {
  count      = var.key_sync_service_account_unique_id != "" ? 1 : 0
  role       = "${var.environment}-key-sync-assume-role"
  policy_arn = one(aws_iam_policy.kek_key_sync_policy[*].arn)
}
