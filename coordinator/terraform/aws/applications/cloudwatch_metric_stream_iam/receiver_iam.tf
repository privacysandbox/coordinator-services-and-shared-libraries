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

locals {
  iam_prefix = "${var.environment}-metric-stream-receiver"
}

# Access for Cloud Watch Stream Receiver service in Cloud Run,
# to read and write the Firehose HTTP Endpoint Access Key Secret.
data "aws_iam_policy_document" "metric_stream_receiver" {
  statement {
    actions = [
      "secretsmanager:DescribeSecret",
      "secretsmanager:GetSecretValue",
      "secretsmanager:ListSecretVersionIds",
      "secretsmanager:PutSecretValue",
    ]
    resources = [local.firehose_secret_arn]
  }
}

data "aws_iam_policy_document" "metric_stream_receiver_assume" {
  statement {
    effect  = "Allow"
    actions = ["sts:AssumeRoleWithWebIdentity"]

    condition {
      test     = "StringEquals"
      variable = "accounts.google.com:email"
      values   = [var.service_account_email]
    }

    condition {
      test = "StringEquals"
      # Use oaud not aud. Where azp is set, aud matches azp instead.
      # https://docs.aws.amazon.com/IAM/latest/UserGuide/reference_policies_iam-condition-keys.html
      variable = "accounts.google.com:oaud"
      values   = ["https://sts.amazonaws.com"]
    }

    principals {
      type        = "Federated"
      identifiers = ["accounts.google.com"]
    }

  }
}

resource "aws_iam_role" "metric_stream_receiver" {
  name                 = "${local.iam_prefix}-role"
  max_session_duration = 3600 # 1h
  assume_role_policy   = data.aws_iam_policy_document.metric_stream_receiver_assume.json
}

resource "aws_iam_role_policy" "metric_stream_receiver" {
  name   = "${local.iam_prefix}-policy"
  role   = aws_iam_role.metric_stream_receiver.id
  policy = data.aws_iam_policy_document.metric_stream_receiver.json
}
