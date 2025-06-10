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

data "aws_caller_identity" "current" {}

locals {
  # Chunk PCR0 allowlist and create separate attestation policy for each chunk to avoid exceeding
  # max IAM policy size.
  policy_name_to_allowlist_chunk = {
    for index, chunk in tolist(chunklist(var.aws_attestation_pcr_allowlist, 54)) :
    "${var.environment}-assumed-role-policy-pcr-list-${index + 1}" => chunk
  }
  aws_account_ids = toset(keys(var.aws_account_id_to_role_names))

  # Create every combination of account id and chunked policy.
  account_id_to_policy_attachment_map = {
    for pair in setproduct(local.aws_account_ids, keys(local.policy_name_to_allowlist_chunk)) :
    "${pair[0]}.${pair[1]}" => {
      account_id  = pair[0],
      policy_name = pair[1],
    }
  }
}

resource "aws_iam_role" "assumed_role" {
  for_each = local.aws_account_ids

  # Warning: Operators rely on the format of this name, it is set automatically on the operator side.
  name               = "${var.environment}-assumed-role-${each.key}"
  assume_role_policy = data.aws_iam_policy_document.assume_role[each.key].json
}

data "aws_iam_policy_document" "assume_role" {
  for_each = local.aws_account_ids

  statement {
    effect  = "Allow"
    actions = ["sts:AssumeRole"]
    # We could use role ARNs as the principals, but the policy will break when
    # the roles are re-created. Using condition instead to avoid this.
    principals {
      type        = "AWS"
      identifiers = [each.key]
    }

    # Note: if roles set is empty no conditions will be created, effectively
    # granting access to the whole account.
    dynamic "condition" {
      # Create the condition block if and only if there are role names specified for this account.
      # This ensures an "OR" logic for multiple roles by listing them in a single "ArnEquals" condition.
      for_each = length(var.aws_account_id_to_role_names[each.key]) > 0 ? [""] : []

      content {
        test     = "ArnEquals"
        variable = "aws:PrincipalArn"
        # Build the list of role ARNs for the current account (each.key)
        values = [
          for role_name in var.aws_account_id_to_role_names[each.key] : "arn:aws:iam::${each.key}:role/${role_name}"
        ]
      }
    }
  }
}

resource "aws_iam_policy" "attestation" {
  for_each = var.aws_attestation_enabled ? local.policy_name_to_allowlist_chunk : {}

  name   = each.key
  policy = data.aws_iam_policy_document.attestation[each.key].json
}

data "aws_iam_policy_document" "attestation" {
  for_each = var.aws_attestation_enabled ? local.policy_name_to_allowlist_chunk : {}

  statement {
    actions = [
      "kms:Decrypt"
    ]
    resources = [var.aws_key_encryption_key_arn]
    condition {
      test     = "StringEqualsIgnoreCase"
      variable = "kms:RecipientAttestation:PCR0"
      values   = each.value
    }
  }
}

resource "aws_iam_role_policy_attachment" "attestation" {
  for_each = var.aws_attestation_enabled ? local.account_id_to_policy_attachment_map : {}

  role       = aws_iam_role.assumed_role[each.value.account_id].name
  policy_arn = "arn:aws:iam::${data.aws_caller_identity.current.account_id}:policy/${each.value.policy_name}"

  depends_on = [aws_iam_policy.attestation]
}

resource "aws_iam_policy" "no_attestation" {
  name  = "${var.environment}-assumed-role-policy-no-attestation"
  count = !var.aws_attestation_enabled ? 1 : 0

  policy = data.aws_iam_policy_document.no_attestation.json
}

data "aws_iam_policy_document" "no_attestation" {
  statement {
    actions = [
      "kms:Decrypt"
    ]
    resources = [var.aws_key_encryption_key_arn]
  }
}

resource "aws_iam_role_policy_attachment" "no_attestation" {
  # Iterate over account ids because assumed_roles are known only after apply.
  for_each = !var.aws_attestation_enabled ? local.aws_account_ids : []

  role       = aws_iam_role.assumed_role[each.key].name
  policy_arn = aws_iam_policy.no_attestation[0].arn
}
