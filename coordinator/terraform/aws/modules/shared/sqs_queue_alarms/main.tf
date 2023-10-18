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

# Total Queue Messages High alarm
resource "aws_cloudwatch_metric_alarm" "total_queue_messages_high_alarm" {
  alarm_name          = "Critical${var.alarm_label_sqs_queue}TotalQueueMessagesHigh${var.custom_alarm_label}"
  alarm_description   = "Total Queue Messages over ${var.total_queue_messages_high_threshold}"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = 1
  #Unit for threshold
  threshold     = var.total_queue_messages_high_threshold
  alarm_actions = [var.sns_topic_arn]
  ok_actions    = [var.sns_topic_arn]


  tags = {
    environment = var.environment
  }
  metric_query {
    id          = "e1"
    expression  = "(m1 + m2)"
    label       = "Total Queue Messages"
    return_data = "true"
  }

  metric_query {
    id = "m1"

    metric {
      metric_name = "ApproximateNumberOfMessagesNotVisible"
      namespace   = "AWS/SQS"
      period      = var.eval_period_sec
      stat        = "Maximum"
      unit        = "Count"
      dimensions = {
        QueueName = var.sqs_queue_name
      }
    }
  }

  metric_query {
    id = "m2"

    metric {
      metric_name = "ApproximateNumberOfMessagesVisible"
      namespace   = "AWS/SQS"
      period      = var.eval_period_sec
      stat        = "Maximum"
      unit        = "Count"
      dimensions = {
        QueueName = var.sqs_queue_name
      }
    }
  }
}
