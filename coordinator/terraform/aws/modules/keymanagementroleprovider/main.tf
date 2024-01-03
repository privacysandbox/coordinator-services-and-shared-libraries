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
  required_version = "~> 1.2.3"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 3.0"
    }
  }
}

# TODO: Rename keymanagementroleprovider to coordinatorroleprovider because this
# provides access to both PBS and key management services now.
locals {
  # List of APIs to grant invoke access on (empty arns are ignored).
  api_arns = tolist([
    var.private_key_api_gateway_arn,
    var.privacy_budget_api_gateway_arn,
  ])
  api_policy_statements = tolist([for arn in local.api_arns : {
    Action = [
      "execute-api:Invoke"
    ],
    Effect   = "Allow",
    Resource = arn
  } if arn != ""])
}

# KMS Decrypt policy contains attestation condition keys (enclave hashes) only if attestation_condition_keys variable is not empty.
# attestation_condition_keys map key is the Condition type (eg ImageSha384) while the value list is the actual hash string.
# For enclave key information https://docs.aws.amazon.com/kms/latest/developerguide/policy-conditions.html#conditions-nitro-enclaves
# If attestation_condition_keys map value contains more than one, then the 'StringEqualsIgnoreCase' is an OR operation the list of strings
# For multiple condition key logic see: https://docs.aws.amazon.com/IAM/latest/UserGuide/reference_policies_multi-value-conditions.html
resource "aws_iam_policy" "coordinator_assume_role_policy" {
  name = "${var.environment}_coordinator_assume_role_policy"

  # Ternary operation is done after json encoding due to terraform limitations
  policy = (length(var.attestation_condition_keys) > 0 ? jsonencode({
    Version = "2012-10-17",
    Statement = concat(local.api_policy_statements, [{
      Action = [
        "kms:Decrypt"
      ],
      Effect   = "Allow",
      Resource = var.private_key_encryptor_arn,
      Condition = {
        StringEqualsIgnoreCase = var.attestation_condition_keys
      }
    }])
    }) :
    jsonencode({
      Version = "2012-10-17",
      Statement = concat(local.api_policy_statements, [{
        Action = [
          "kms:Decrypt"
        ],
        Effect   = "Allow",
        Resource = var.private_key_encryptor_arn,
      }])
  }))
}

# Create a role for each allowed_principal
resource "aws_iam_role" "coordinator_assume_role" {
  for_each = var.allowed_principals_map_v2

  name = "${var.environment}_${each.key}_coordinator_assume_role"
  assume_role_policy = jsonencode({
    Version = "2012-10-17",
    Statement = [
      {
        Action    = "sts:AssumeRole"
        Condition = {}
        Effect    = "Allow"
        Principal = {
          AWS = each.key
        }
      },
    ]
  })
}

# Attach each created role to the same policy
resource "aws_iam_role_policy_attachment" "coordinator_assume_role_policy" {
  for_each = aws_iam_role.coordinator_assume_role

  role       = each.value.name
  policy_arn = aws_iam_policy.coordinator_assume_role_policy.arn
}
