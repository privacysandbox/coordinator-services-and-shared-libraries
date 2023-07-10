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

# Content of this queue are generic triggers for key generation job with no sensitive data.
#tfsec:ignore:aws-sqs-enable-queue-encryption
resource "aws_sqs_queue" "key_job_queue" {
  name = "${var.environment}-${var.key_job_queue_name}"

  fifo_queue                  = true
  content_based_deduplication = true
  deduplication_scope         = "queue"
  sqs_managed_sse_enabled     = true

  # 14 days, AWS maximum
  message_retention_seconds  = 1209600
  visibility_timeout_seconds = 600 // 10 minutes

  # Tags for identifying the queue in various metrics
  tags = {
    name        = var.key_job_queue_name
    service     = "split-key-service"
    environment = var.environment
    role        = "job_queue"
  }
}

resource "aws_sqs_queue_policy" "sqs_event_policy" {
  queue_url = aws_sqs_queue.key_job_queue.id

  policy = jsonencode({
    "Version" : "2012-10-17",
    "Statement" : [
      {
        Action : [
          "sqs:SendMessage",
        ],
        Resource : aws_sqs_queue.key_job_queue.arn
        Effect : "Allow"
        Principal : {
          "Service" : "events.amazonaws.com"
        }
      }
    ]
  })
}

resource "aws_cloudwatch_event_rule" "key_rotation_rule" {
  name                = "${var.environment}_split-key-rotation-rule"
  schedule_expression = "rate(6 hours)"
}

resource "aws_cloudwatch_event_target" "key_rotation_event_target" {
  rule = aws_cloudwatch_event_rule.key_rotation_rule.name
  sqs_target {
    message_group_id = aws_sqs_queue.key_job_queue.id
  }
  arn = aws_sqs_queue.key_job_queue.arn
}
