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

################################################################################
# VPC Endpoints and Routes
################################################################################

data "aws_iam_policy_document" "dynamodb_vpce_policy" {
  statement {
    sid       = "AccessToDynamoDb"
    effect    = "Allow"
    resources = [var.dynamodb_arn]
    actions   = ["dynamodb:*"]
    principals {
      identifiers = ["*"]
      type        = "AWS"
    }

    # Note: The `aws:PrincipalArn` needs to match the principal that made
    # the request with the ARN that you specify in the policy. For IAM roles, the
    # request context returns the ARN of the role, not the ARN of the user that
    # assumed the role.  See https://docs.aws.amazon.com/IAM/latest/UserGuide/reference_policies_condition-keys.html#condition-keys-principalarn
    condition {
      test     = "StringLike"
      values   = var.dynamodb_allowed_principal_arns
      variable = "aws:PrincipalArn"
    }
  }
}

resource "aws_vpc_endpoint" "dynamodb_endpoint" {
  count = var.enable_dynamodb_vpc_endpoint ? 1 : 0

  vpc_id       = aws_vpc.main.id
  service_name = "com.amazonaws.${local.region}.dynamodb"
  policy       = data.aws_iam_policy_document.dynamodb_vpce_policy.json

  tags = merge(
    {
      Name        = "${var.environment}-dynamodb-vpce"
      environment = var.environment
      subnet_type = "private"
    },
    var.vpc_default_tags
  )
}

data "aws_iam_policy_document" "kms_vpce_policy" {
  statement {
    sid       = "AccessToSpecificKmsKeys"
    effect    = "Allow"
    resources = var.kms_keys_arns
    actions   = ["kms:*"]
    principals {
      identifiers = ["*"]
      type        = "AWS"
    }

    # Note: The `aws:PrincipalArn` needs to match the principal that made
    # the request with the ARN that you specify in the policy. For IAM roles, the
    # request context returns the ARN of the role, not the ARN of the user that
    # assumed the role.  See https://docs.aws.amazon.com/IAM/latest/UserGuide/reference_policies_condition-keys.html#condition-keys-principalarn
    condition {
      test     = "StringLike"
      values   = var.kms_allowed_principal_arns
      variable = "aws:PrincipalArn"
    }
  }
}

resource "aws_vpc_endpoint" "kms_endpoint" {
  count = var.enable_kms_vpc_endpoint ? 1 : 0

  vpc_id            = aws_vpc.main.id
  vpc_endpoint_type = "Interface"
  service_name      = "com.amazonaws.${local.region}.kms"
  subnet_ids        = aws_subnet.private[*].id
  security_group_ids = [
    aws_security_group.allow_internal_sg.id,
    aws_security_group.vpc_egress_sg.id,
  ]

  # Enables the standard AWS KMS DNS hostname (https://kms.<region>.amazonaws.com)
  # to resolves to this VPC endpoint. See more https://docs.aws.amazon.com/vpc/latest/privatelink/manage-dns-names.html
  private_dns_enabled = true

  policy = data.aws_iam_policy_document.kms_vpce_policy.json

  tags = merge(
    {
      Name        = "${var.environment}-kms-vpce"
      environment = var.environment
      subnet_type = "private"
    },
    var.vpc_default_tags
  )
}
