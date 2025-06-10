# Copyright 2024 Google LLC
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
      version = ">= 3.0"
    }
  }
}

resource "aws_kms_key" "key_encryption_key" {
  description         = "${var.environment}-key-encryption-key"
  enable_key_rotation = true
  multi_region        = true

  lifecycle {
    prevent_destroy = true
  }
}

resource "aws_kms_alias" "key_encryption_key_alias" {
  name          = "alias/${var.environment}-key-encryption-key"
  target_key_id = aws_kms_key.key_encryption_key.id
}

# A role allowing GCP key generation instance access to encrypt with AWS key encryption key.
resource "aws_iam_role" "key_encryption_key_role" {
  name = "${var.environment}-key-encryption-key-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17",
    Statement = [
      {
        "Effect" : "Allow",
        "Principal" : { "Federated" : "accounts.google.com" },
        "Action" : "sts:AssumeRoleWithWebIdentity",
        "Condition" : {
          # A token will contain unique service account ID in the sub field.
          "StringEquals" : {
            "accounts.google.com:sub" : var.key_encryptor_service_account_unique_id
          }
        },
      }
    ]
  })
}

resource "aws_iam_policy" "key_encryption_key_policy" {
  name        = "${var.environment}-key-encryption-key-policy"
  path        = "/"
  description = "IAM policy allowing to encrypt with AWS key encryption key"

  policy = jsonencode({
    Version = "2012-10-17",
    Statement = [
      {
        "Action" : [
          "kms:Encrypt"
        ]
        "Resource" : aws_kms_key.key_encryption_key.arn
        "Effect" : "Allow"
      }
    ]
  })
}

resource "aws_iam_role_policy_attachment" "key_encryption_key_policy_attachment" {
  role       = aws_iam_role.key_encryption_key_role.name
  policy_arn = aws_iam_policy.key_encryption_key_policy.arn
}
