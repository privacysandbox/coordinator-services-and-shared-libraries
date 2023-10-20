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
  sns_topic_name = "${var.environment}_pbsprober_sns_topic"
  use_sns_to_sqs = var.alarms_sns_topic_arn != "" && var.alarms_sqs_queue_arn != ""
}

resource "aws_sns_topic" "pbsprober" {
  count = var.alarms_enabled ? 1 : 0
  name  = local.sns_topic_name
  tags = {
    environment = var.environment
  }
}

resource "aws_sns_topic_subscription" "pbsprober" {
  count     = var.alarms_enabled ? 1 : 0
  topic_arn = local.use_sns_to_sqs ? var.alarms_sns_topic_arn : aws_sns_topic.pbsprober[0].arn
  protocol  = local.use_sns_to_sqs ? "sqs" : "email"
  endpoint  = local.use_sns_to_sqs ? var.alarms_sqs_queue_arn : var.alarms_notification_email
}

resource "aws_cloudwatch_metric_alarm" "pbsprober_lambda_error_alarm" {
  count = var.alarms_enabled ? 1 : 0

  alarm_name          = "WarningProberError${var.custom_alarm_label}"
  alarm_description   = "PBS Prober Lambda errors over 5%"
  comparison_operator = "GreaterThanOrEqualToThreshold"
  threshold           = 5
  evaluation_periods  = 1
  datapoints_to_alarm = 1
  treat_missing_data  = "breaching"

  metric_query {
    id = "errorCount"
    metric {
      metric_name = "Errors"
      namespace   = "AWS/Lambda"
      period      = 60 * 100
      stat        = "Sum"
      dimensions = {
        FunctionName = aws_lambda_function.pbsprober_lambda.function_name
      }
    }
  }

  metric_query {
    id = "invocations"
    metric {
      metric_name = "Invocations"
      namespace   = "AWS/Lambda"
      period      = 60 * 100
      stat        = "Sum"
      dimensions = {
        FunctionName = aws_lambda_function.pbsprober_lambda.function_name
      }
    }
  }

  metric_query {
    id          = "errorRate"
    expression  = "(errorCount / invocations) * 100"
    label       = "Lambda error rate percentage (from 0 to 100 inclusive)"
    return_data = "true"
  }

  alarm_actions = [
    local.use_sns_to_sqs ? var.alarms_sns_topic_arn : aws_sns_topic.pbsprober[0].arn
  ]
  ok_actions = [
    local.use_sns_to_sqs ? var.alarms_sns_topic_arn : aws_sns_topic.pbsprober[0].arn
  ]

  depends_on = [
    aws_sns_topic.pbsprober[0],
    aws_lambda_function.pbsprober_lambda,
  ]
}
