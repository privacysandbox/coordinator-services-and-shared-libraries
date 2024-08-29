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

provider "aws" {
  region = var.aws_region
}

locals {
  heartbeat_name_prefix = var.heartbeat_name_prefix != "" ? var.heartbeat_name_prefix : lower(var.alarms_service_label)
  use_sns_to_sqs        = var.alarms_sns_topic_arn != "" && var.alarms_sqs_queue_arn != ""
}

data "aws_caller_identity" "current" {}

data "aws_iam_policy_document" "canary-assume-role-policy" {
  statement {
    actions = ["sts:AssumeRole"]
    effect  = "Allow"

    principals {
      identifiers = ["lambda.amazonaws.com"]
      type        = "Service"
    }
  }
}

resource "aws_iam_role" "canary-role" {
  name               = "${var.environment}-${var.heartbeat_name}-canary-role"
  path               = "/${var.heartbeat_name}-com/"
  assume_role_policy = data.aws_iam_policy_document.canary-assume-role-policy.json
  description        = "IAM role for AWS Synthetic Monitoring Canaries of ${var.heartbeat_name}."
}

data "aws_iam_policy_document" "canary-policy" {
  statement {
    sid    = "CanaryGeneric"
    effect = "Allow"
    actions = [
      "s3:PutObject",
      "s3:GetObject",
      "s3:GetBucketLocation",
      "s3:ListAllMyBuckets",
      "cloudwatch:PutMetricData",
      "logs:CreateLogGroup",
      "logs:CreateLogStream",
      "logs:PutLogEvents",
      "xray:PutTraceSegments"
    ]
    resources = ["*"]
  }
}

data "aws_iam_policy_document" "allowed-assume-role-policy" {
  count = var.allowed_assume_role != "" ? 1 : 0
  statement {
    sid       = "AllowedAssumeRole"
    actions   = ["sts:AssumeRole"]
    effect    = "Allow"
    resources = [var.allowed_assume_role]
  }
}

resource "aws_iam_policy" "canary-policy" {
  name        = "${var.environment}-${var.heartbeat_name}-canary-policy"
  path        = "/${var.heartbeat_name}-com/"
  policy      = data.aws_iam_policy_document.canary-policy.json
  description = "IAM role for AWS Synthetic Monitoring Canaries"
}

resource "aws_iam_policy" "allowed-assume-role-policy" {
  count       = var.allowed_assume_role != "" ? 1 : 0
  name        = "${var.environment}-${var.heartbeat_name}-allowed-assume-role-policy"
  path        = "/${var.heartbeat_name}-com/"
  policy      = data.aws_iam_policy_document.allowed-assume-role-policy[0].json
  description = "IAM role policy for AWS Synthetic Monitoring Canaries used for authenticatation."
}

resource "aws_iam_role_policy_attachment" "canary-policy-attachment" {
  role       = aws_iam_role.canary-role.name
  policy_arn = aws_iam_policy.canary-policy.arn
}

resource "aws_iam_role_policy_attachment" "allowed-assume-role-policy-attachment" {
  count      = var.allowed_assume_role != "" ? 1 : 0
  role       = aws_iam_role.canary-role.name
  policy_arn = aws_iam_policy.allowed-assume-role-policy[0].arn
}

resource "aws_s3_bucket" "heartbeat-canary" {
  bucket = "${var.environment}-${var.heartbeat_name}-canary"

  # TODO: Remove once AWS provider is upgraded to 4.9.0+.
  lifecycle {
    ignore_changes = [
      grant,
      lifecycle_rule,
    ]
  }

  tags = {
    Name        = "${var.heartbeat_name}-canary"
    Application = "${var.heartbeat_name}"
    Environment = "${var.environment}"
  }
}

resource "aws_s3_bucket_lifecycle_configuration" "heartbeat-canary" {
  bucket = aws_s3_bucket.heartbeat-canary.id

  rule {
    id     = "tf-s3-lifecycle-20230309234714009300000001"
    status = "Enabled"

    noncurrent_version_expiration {
      noncurrent_days = 60
    }
  }
}

resource "aws_s3_bucket_versioning" "heartbeat-canary" {
  bucket = aws_s3_bucket.heartbeat-canary.id

  versioning_configuration {
    status = "Enabled"
  }
}

resource "aws_s3_bucket_public_access_block" "heartbeat-canary" {
  bucket = aws_s3_bucket.heartbeat-canary.id

  block_public_acls       = true
  block_public_policy     = true
  restrict_public_buckets = true
  ignore_public_acls      = true
}

resource "aws_s3_bucket_policy" "heartbeat-canary-policy" {
  bucket = aws_s3_bucket.heartbeat-canary.id
  policy = jsonencode({
    Version = "2012-10-17"
    Id      = "${var.heartbeat_name}CanaryPolicy"
    Statement = [
      {
        Sid    = "Permissions"
        Effect = "Allow"
        Principal = {
          AWS = data.aws_caller_identity.current.account_id
        }
        Action   = ["s3:*"]
        Resource = ["${aws_s3_bucket.heartbeat-canary.arn}/*"]
      }
    ]
  })
}

resource "aws_sns_topic" "heartbeat-sns-topic" {
  name = "${var.heartbeat_name}${var.environment}_sns_topic"
}

resource "aws_sns_topic_subscription" "heartbeat-sns-topic-subscription" {
  topic_arn = local.use_sns_to_sqs ? var.alarms_sns_topic_arn : aws_sns_topic.heartbeat-sns-topic.arn
  protocol  = local.use_sns_to_sqs ? "sqs" : "email"
  endpoint  = local.use_sns_to_sqs ? var.alarms_sqs_queue_arn : var.alarms_notification_email
}

resource "aws_s3_object" "canary_function_zip" {
  bucket      = aws_s3_bucket.heartbeat-canary.id
  key         = "canary-function-${filemd5(var.heartbeat_source_zip)}.zip"
  acl         = "private"
  source      = var.heartbeat_source_zip
  source_hash = filemd5(var.heartbeat_source_zip)
}

resource "aws_synthetics_canary" "heartbeat" {
  depends_on           = [aws_s3_object.canary_function_zip]
  name                 = "${local.heartbeat_name_prefix}-${var.environment}"
  artifact_s3_location = "s3://${aws_s3_bucket.heartbeat-canary.id}"
  execution_role_arn   = aws_iam_role.canary-role.arn
  handler              = var.heartbeat_handler
  s3_bucket            = aws_s3_bucket.heartbeat-canary.id
  s3_key               = aws_s3_object.canary_function_zip.key
  runtime_version      = "syn-python-selenium-4.0"

  run_config {
    timeout_in_seconds = var.heartbeat_timeout_sec
    environment_variables = {
      ALLOWED_ASSUME_ROLE = var.allowed_assume_role
      URL_TO_PROBE        = var.url_to_probe
    }
  }

  schedule {
    expression = var.heartbeat_schedule_expression
  }
  start_canary = true
}

resource "aws_cloudwatch_metric_alarm" "Synthetics-Alarm" {
  count                     = var.alarms_enabled ? 1 : 0
  alarm_name                = "${var.alarms_priority}${var.alarms_service_label}HeartbeatProber${var.custom_alarm_label}"
  comparison_operator       = "LessThanThreshold"
  evaluation_periods        = var.alarms_eval_periods
  metric_name               = "SuccessPercent"
  namespace                 = "CloudWatchSynthetics"
  threshold                 = var.alarms_threshold
  statistic                 = "Average"
  period                    = var.alarms_eval_period_sec
  alarm_description         = "Synthetics alarm metric: SuccessPercent LessThanThreshold 90"
  insufficient_data_actions = []
  alarm_actions             = [var.alarms_sns_topic_arn]

  dimensions = {
    CanaryName = aws_synthetics_canary.heartbeat.name
  }
}
