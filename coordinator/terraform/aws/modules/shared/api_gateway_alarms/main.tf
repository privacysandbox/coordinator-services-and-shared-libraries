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

# Latency alarm
resource "aws_cloudwatch_metric_alarm" "max_latency_alarm" {
  alarm_name          = "Info${var.api_name}ApiGatewayMaxLatency${var.custom_alarm_label}"
  alarm_description   = "Max latency over ${var.max_latency_ms}ms"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = 1
  metric_name         = "Latency"
  namespace           = "AWS/ApiGateway"
  period              = var.eval_period_sec
  #Unit for threshold
  unit      = "Milliseconds"
  threshold = var.max_latency_ms
  statistic = "Maximum"

  dimensions = {
    ApiId = var.api_gateway_id
  }
  alarm_actions = [var.sns_topic_arn]
  ok_actions    = [var.sns_topic_arn]

  tags = {
    environment = var.environment
  }
}

# 5xx Errors alarm
resource "aws_cloudwatch_metric_alarm" "error_5xx_alarm" {
  alarm_name                = "Warning${var.api_name}ApiGateway5xx${var.custom_alarm_label}"
  alarm_description         = "5xx errors over ${var.error_5xx_threshold}%"
  comparison_operator       = "GreaterThanThreshold"
  evaluation_periods        = 1
  threshold                 = var.error_5xx_threshold
  insufficient_data_actions = []
  metric_name               = "5xx"
  namespace                 = "AWS/ApiGateway"
  period                    = var.eval_period_sec
  statistic                 = "Sum"

  treat_missing_data = "notBreaching"

  dimensions = {
    ApiId = var.api_gateway_id
  }
  alarm_actions = [var.sns_topic_arn]
  ok_actions    = [var.sns_topic_arn]

  tags = {
    environment = var.environment
  }
}
