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

terraform {
  required_version = "~> 1.2.3"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 3.0"
    }
  }
}

# Cloudfront monitoring subscription allows for additional metrics to be enabled and logged
# These metrics include 4xx and 5xx error rates and origin latency
resource "aws_cloudfront_monitoring_subscription" "cloudfront_monitoring" {
  distribution_id = var.cloudfront_distribution_id

  monitoring_subscription {
    realtime_metrics_subscription_config {
      realtime_metrics_subscription_status = "Enabled"
    }
  }
}

#5xx Error alarm
resource "aws_cloudwatch_metric_alarm" "cloudfront_5xx_alarm" {
  alarm_name          = "Warning${var.cloudfront_alarm_name_prefix}Cloudfront5xx${var.custom_alarm_label}"
  alarm_description   = "5xx errors ratio over ${var.cloudfront_5xx_threshold}"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = var.cloudfront_5xx_eval_periods
  metric_name         = "5xxErrorRate"
  namespace           = "AWS/CloudFront"
  threshold           = var.cloudfront_5xx_threshold
  period              = "60"
  statistic           = "Average"
  treat_missing_data  = "notBreaching"
  actions_enabled     = true

  dimensions = {
    DistributionId = var.cloudfront_distribution_id

    Region = "Global"
  }

  alarm_actions = [var.sns_topic_arn]
  ok_actions    = [var.sns_topic_arn]
}

#Alarms when cache hit ratio dips below certain level
resource "aws_cloudwatch_metric_alarm" "cloudfront_cache_hit_alarm" {
  alarm_name          = "Warning${var.cloudfront_alarm_name_prefix}CloudfrontCacheHit${var.custom_alarm_label}"
  alarm_description   = "Cache hit percentage less than ${var.cloudfront_cache_hit_threshold}%"
  comparison_operator = "LessThanThreshold"
  evaluation_periods  = var.cloudfront_cache_hit_eval_periods
  metric_name         = "CacheHitRate"
  namespace           = "AWS/CloudFront"
  threshold           = var.cloudfront_cache_hit_threshold
  period              = "60"
  statistic           = "Average"
  actions_enabled     = true

  dimensions = {
    DistributionId = var.cloudfront_distribution_id

    Region = "Global"
  }

  alarm_actions = [var.sns_topic_arn]
  ok_actions    = [var.sns_topic_arn]
}

# totalQPS > 100 then measure, otherwise ignore
resource "aws_cloudwatch_metric_alarm" "cloudfront_origin_latency_alarm" {
  alarm_name                = "Warning${var.cloudfront_alarm_name_prefix}CloudfrontOriginLatency${var.custom_alarm_label}"
  alarm_description         = "Max origin latency over ${var.cloudfront_origin_latency_threshold}ms with minimum 100 QPS"
  comparison_operator       = "GreaterThanThreshold"
  evaluation_periods        = var.cloudfront_origin_latency_eval_periods
  threshold                 = var.cloudfront_origin_latency_threshold
  actions_enabled           = true
  alarm_actions             = [var.sns_topic_arn]
  ok_actions                = [var.sns_topic_arn]
  insufficient_data_actions = []

  metric_query {
    id = "e1"
    # Setting a minimum request count to remove the noise of low qps. Min 100QPS
    expression  = "IF(m1>6000,m2,0)"
    label       = "Minimum QPS Origin Latency Alarm"
    return_data = "true"
  }

  metric_query {
    id = "m1"

    metric {
      metric_name = "RequestCount"
      namespace   = "AWS/CloudFront"
      period      = "60"
      stat        = "Sum"
      unit        = "Count"
      dimensions = {
        DistributionId = var.cloudfront_distribution_id
      }
    }
  }

  metric_query {
    id = "m2"

    metric {
      metric_name = "OriginLatency"
      namespace   = "AWS/CloudFront"
      period      = "60"
      stat        = "p95"
      unit        = "Milliseconds"
      dimensions = {
        DistributionId = var.cloudfront_distribution_id
      }
    }
  }
}
