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

# Cloudfront monitoring subscription allows for additional metrics to be enabled and logged
# These metrics include 4xx and 5xx error rates and origin latency
resource "aws_cloudfront_monitoring_subscription" "get_public_key_cloudfront_monitoring" {
  distribution_id = aws_cloudfront_distribution.get_public_keys_cloudfront.id

  monitoring_subscription {
    realtime_metrics_subscription_config {
      realtime_metrics_subscription_status = "Enabled"
    }
  }
}

#5xx Error alarm
resource "aws_cloudwatch_metric_alarm" "get_public_key_cloudfront_5xx_alarm" {
  count               = var.public_key_service_alarms_enabled ? 1 : 0
  alarm_name          = "WarningGetPublicKeyCloudfront5xx${var.custom_alarm_label}"
  alarm_description   = "5xx errors ratio over ${var.get_public_key_cloudfront_5xx_threshold}"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = 1
  metric_name         = "5xxErrorRate"
  namespace           = "AWS/CloudFront"
  threshold           = var.get_public_key_cloudfront_5xx_threshold
  period              = var.get_public_key_alarm_eval_period_sec
  statistic           = "Average"
  treat_missing_data  = "notBreaching"
  actions_enabled     = true
  datapoints_to_alarm = 1

  dimensions = {
    DistributionId = aws_cloudfront_distribution.get_public_keys_cloudfront.id
    Region         = "Global"
  }

  alarm_actions = [var.get_public_key_sns_topic_arn]
  ok_actions    = [var.get_public_key_sns_topic_arn]
}

#Alarms when cache hit ratio dips below certain level
resource "aws_cloudwatch_metric_alarm" "get_public_key_cloudfront_cache_hit_alarm" {
  count               = var.public_key_service_alarms_enabled ? 1 : 0
  alarm_name          = "WarningGetPublicKeyCloudfrontCacheHit${var.custom_alarm_label}"
  alarm_description   = "Cache hit percentage less than ${var.get_public_key_cloudfront_cache_hit_threshold}%"
  comparison_operator = "LessThanThreshold"
  evaluation_periods  = 1
  metric_name         = "CacheHitRate"
  namespace           = "AWS/CloudFront"
  threshold           = var.get_public_key_cloudfront_cache_hit_threshold
  period              = var.get_public_key_alarm_eval_period_sec
  statistic           = "Average"
  actions_enabled     = true

  dimensions = {
    DistributionId = aws_cloudfront_distribution.get_public_keys_cloudfront.id
    Region         = "Global"
  }

  alarm_actions = [var.get_public_key_sns_topic_arn]
  ok_actions    = [var.get_public_key_sns_topic_arn]
}

#Origin latency alarm
resource "aws_cloudwatch_metric_alarm" "get_public_key_cloudfront_origin_latency_alarm" {
  count               = var.public_key_service_alarms_enabled ? 1 : 0
  alarm_name          = "WarningGetPublicKeyCloudfrontOriginLatency${var.custom_alarm_label}"
  alarm_description   = "Max origin latency over ${var.get_public_key_cloudfront_origin_latency_threshold}ms"
  comparison_operator = "GreaterThanThreshold"
  evaluation_periods  = 1
  metric_name         = "OriginLatency"
  namespace           = "AWS/CloudFront"
  threshold           = var.get_public_key_cloudfront_origin_latency_threshold
  period              = var.get_public_key_alarm_eval_period_sec
  statistic           = "Maximum"
  actions_enabled     = true

  dimensions = {
    DistributionId = aws_cloudfront_distribution.get_public_keys_cloudfront.id
    Region         = "Global"
  }

  alarm_actions = [var.get_public_key_sns_topic_arn]
  ok_actions    = [var.get_public_key_sns_topic_arn]
}
