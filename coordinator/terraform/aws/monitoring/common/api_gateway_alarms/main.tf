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
  alarm_description   = "Max latency over ${var.max_latency_ms_threshold}ms"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = var.max_latency_eval_periods
  metric_name         = "Latency"
  namespace           = "AWS/ApiGateway"
  period              = "60"
  # Unit for threshold
  unit      = "Milliseconds"
  threshold = var.max_latency_ms_threshold
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

# api_gateway_4xx_error_ratio_high(4XXerror/TotalCount)
resource "aws_cloudwatch_metric_alarm" "api_gateway_4xx_error_ratio_high" {
  alarm_name                = "Warning${var.api_name}ApiGateway4xxErrorRatioHigh${var.custom_alarm_label}"
  comparison_operator       = "GreaterThanOrEqualToThreshold"
  evaluation_periods        = var.error_ratio_4xx_eval_periods
  threshold                 = var.error_ratio_4xx_threshold
  alarm_description         = "4XXError rate ratio has exceeded ${var.error_ratio_4xx_threshold}%"
  insufficient_data_actions = []
  alarm_actions             = [var.sns_topic_arn]
  ok_actions                = [var.sns_topic_arn]

  tags = {
    environment = var.environment
  }

  metric_query {
    id          = "e1"
    expression  = "(m2/m1)*100"
    label       = "Error Rate Ratio"
    return_data = "true"
  }

  metric_query {
    id = "m1"

    metric {
      metric_name = "Count"
      namespace   = "AWS/ApiGateway"
      period      = "60"
      stat        = "Sum"
      unit        = "Count"
      dimensions = {
        AppId = var.api_gateway_id
      }
    }
  }

  metric_query {
    id = "m2"

    metric {
      metric_name = "4XXError"
      namespace   = "AWS/ApiGateway"
      period      = "60"
      stat        = "Sum"
      unit        = "Count"
      dimensions = {
        AppId = var.api_gateway_id
      }
    }
  }
}

# api_gateway_5xx_error_ratio_high(5XXerror/TotalCount)
resource "aws_cloudwatch_metric_alarm" "api_gateway_5xx_error_ratio_high" {
  alarm_name                = "Warning${var.api_name}ApiGateway5xxErrorRatioHigh${var.custom_alarm_label}"
  comparison_operator       = "GreaterThanOrEqualToThreshold"
  evaluation_periods        = var.error_ratio_5xx_eval_periods
  threshold                 = var.error_ratio_5xx_threshold
  alarm_description         = "5XXError rate ratio has exceeded ${var.error_ratio_5xx_threshold}%"
  insufficient_data_actions = []
  alarm_actions             = [var.sns_topic_arn]
  ok_actions                = [var.sns_topic_arn]

  tags = {
    environment = var.environment
  }

  metric_query {
    id          = "e1"
    expression  = "(m2/m1)*100"
    label       = "Error Rate Ratio"
    return_data = "true"
  }

  metric_query {
    id = "m1"

    metric {
      metric_name = "Count"
      namespace   = "AWS/ApiGateway"
      period      = "60"
      stat        = "Sum"
      unit        = "Count"
      dimensions = {
        AppId = var.api_gateway_id
      }
    }
  }

  metric_query {
    id = "m2"

    metric {
      metric_name = "5XXError"
      namespace   = "AWS/ApiGateway"
      period      = "60"
      stat        = "Sum"
      unit        = "Count"
      dimensions = {
        AppId = var.api_gateway_id
      }
    }
  }
}
