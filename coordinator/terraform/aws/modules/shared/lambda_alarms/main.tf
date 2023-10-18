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

locals {
  # Prefix for alarms/metrics created in this module.
  resource_prefix         = var.lambda_function_name
  error_log_metric_name   = "${local.resource_prefix}_error_log_count"
  custom_metric_namespace = "${var.environment}/scp/LambdaErrors"
}

##################
# Custom Metrics #
##################

# Metric for logs which contain "ERROR".
resource "aws_cloudwatch_log_metric_filter" "lambda_error_metric" {
  name           = local.error_log_metric_name
  pattern        = "ERROR"
  log_group_name = var.cloudwatch_log_group_name

  metric_transformation {
    name      = local.error_log_metric_name
    namespace = local.custom_metric_namespace
    value     = "1"
  }
}

##########
# Alarms #
##########

# Alarm on lambda invocations resulting in an error.
resource "aws_cloudwatch_metric_alarm" "lambda_invocation_error_alarm" {
  alarm_name          = "Warning${var.lambda_function_name_alarm}LambdaError${var.custom_alarm_label}"
  alarm_description   = "Lambda errors over ${var.execution_error_threshold}%"
  comparison_operator = "GreaterThanThreshold"
  #Number of 'period' to evaluate for the alarm
  evaluation_periods        = 1
  threshold                 = var.execution_error_threshold
  insufficient_data_actions = []
  metric_name               = "Errors"
  namespace                 = "AWS/Lambda"
  period                    = var.eval_period_sec
  statistic                 = "Sum"

  treat_missing_data = "notBreaching"

  dimensions = {
    FunctionName = var.lambda_function_name
  }
  alarm_actions = [var.sns_topic_arn]
  ok_actions    = [var.sns_topic_arn]

  tags = {
    environment = var.environment
  }
}

# Alarm on the count of logs containing "ERROR" (metric defined in this module).
resource "aws_cloudwatch_metric_alarm" "lambda_error_log_alarm" {
  alarm_name                = "Warning${var.lambda_function_name_alarm}ErrorLog${var.custom_alarm_label}"
  alarm_description         = "Lambda error logs more than ${var.error_log_threshold}"
  comparison_operator       = "GreaterThanThreshold"
  evaluation_periods        = 1
  threshold                 = var.error_log_threshold
  insufficient_data_actions = []
  metric_name               = local.error_log_metric_name
  namespace                 = local.custom_metric_namespace
  period                    = var.eval_period_sec
  statistic                 = "Sum"

  treat_missing_data = "notBreaching"

  alarm_actions = [var.sns_topic_arn]
  ok_actions    = [var.sns_topic_arn]

  tags = {
    environment = var.environment
  }
}

# Alarm on the runtime duration of the lambda.
resource "aws_cloudwatch_metric_alarm" "lambda_max_duration_alarm" {
  alarm_name                = "Warning${var.lambda_function_name_alarm}MaxDuration${var.custom_alarm_label}"
  alarm_description         = "Lambda duration over ${var.max_duration_threshold_ms}ms"
  comparison_operator       = "GreaterThanThreshold"
  evaluation_periods        = 1
  threshold                 = var.max_duration_threshold_ms
  insufficient_data_actions = []
  metric_name               = "Duration"
  namespace                 = "AWS/Lambda"
  period                    = var.eval_period_sec
  statistic                 = "Maximum"

  treat_missing_data = "notBreaching"

  dimensions = {
    FunctionName = var.lambda_function_name
  }
  alarm_actions = [var.sns_topic_arn]
  ok_actions    = [var.sns_topic_arn]

  tags = {
    environment = var.environment
  }
}
