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
  # The aws_cloudwatch_metric_stream resource will need dynamic blocks,
  # both for the include_filter list of metrics, and the statistics_configuration.
  metrics = {
    # Each entry in this map contains a set of metrics in each namespace,
    # that have the same statistics required.
    # (The actual key string is not used.)
    "cumulative" : {
      namespaces = {
        "AWS/ApiGateway" = toset(["4xx", "5xx", "Count"])
        "AWS/ApplicationELB" = toset([
          "RequestCount",
          "HTTPCode_Target_2XX_Count",
          "HTTPCode_Target_4XX_Count",
          "HTTPCode_Target_5XX_Count",
          "HTTPCode_ELB_4XX_Count",
          "HTTPCode_ELB_5XX_Count",
        ])
        "AWS/CloudFront" = toset(["Requests"])
        "AWS/Lambda"     = toset(["Invocations", "Errors"])
      }
      include_mean    = false
      explicit_bounds = []
    },
    "gauges" : {
      namespaces = {
        "AWS/CloudFront" = toset(["4xxErrorRate", "5xxErrorRate", "CacheHitRate"])
        "AWS/SQS"        = toset(["ApproximateAgeOfOldestMessage"])
      }
      include_mean    = true
      explicit_bounds = []
    }
    "latency_ms" : {
      namespaces = {
        "AWS/ApiGateway" = toset(["Latency", "IntegrationLatency"])
        "AWS/CloudFront" = toset(["OriginLatency"])
        "AWS/Lambda"     = toset(["Duration"])
      }
      include_mean = true,
      # Inspired by OTel HTTP semantic conventions
      # https://opentelemetry.io/docs/specs/semconv/http/http-metrics/#metric-httpserverrequestduration
      explicit_bounds = [0, 5, 10, 25, 50, 75, 100, 250, 500, 750, 1000, 2500, 5000, 7500, 10000]
    },
    "latency_s" : {
      namespaces = {
        "AWS/ApplicationELB" = toset(["TargetResponseTime"])
      }
      include_mean    = true
      explicit_bounds = [0, 0.005, 0.01, 0.025, 0.05, 0.075, 0.1, 0.25, 0.5, 0.75, 1, 2.5, 5, 7.5, 10]
    }
  }

  # Find the set of all namespaces, across the different categories of metrics above.
  # Example: toset(["AWS/ApiGateway", "AWS/Lambda", …])
  all_namespaces = toset(flatten([
    for n in [for m in local.metrics : m.namespaces] : keys(n)
  ]))

  # For each namespace, find all of the individual metrics required.
  # Used to generate aws_cloudwatch_metric_stream include_filter blocks.
  # Example:
  # {
  #   "AWS/ApiGateway"     = toset([ "Count", "Latency", …])
  #   "AWS/ApplicationELB" = toset([ "RequestCount", "TargetResponseTime", …])
  #   …
  # }
  all_namespaced_metrics = {
    for n in local.all_namespaces : n => toset(compact(flatten([
      for m in local.metrics : lookup(m.namespaces, n, null)
    ])))
  }

  # For each category of metrics, produce the statistics_configuration block.
  statistics_configuration = {
    for m, value in local.metrics : m => {
      additional_statistics = compact(flatten(concat(
        # CloudWatch Metric Streams don’t give you the mean by default,
        # and don’t support sending the "Average".
        # So, use Trimmed Mean over the entire range from the 0th-percentile
        # to the 100th-percentile.
        # https://docs.aws.amazon.com/AmazonCloudWatch/latest/monitoring/CloudWatch-metric-streams-statistics.html
        # https://docs.aws.amazon.com/AmazonCloudWatch/latest/monitoring/Statistics-definitions.html
        [value.include_mean ? "TM(0%:100%)" : null],

        # Request Trimmed Count from specific bounds, to produce histogram distributions.
        (length(value.explicit_bounds) > 0 ? [
          ["TC(:${element(value.explicit_bounds, 0)})"],
          [for i, bound in value.explicit_bounds :
            "TC(${element(value.explicit_bounds, i)}:${element(value.explicit_bounds, i + 1)})"
            if i < length(value.explicit_bounds) - 1
          ],
          ["TC(${element(value.explicit_bounds, length(value.explicit_bounds) - 1)}:)"],
        ] : [])
      ))),
      include_metric = merge([
        for namespace, metrics in value.namespaces : tomap({
          for metric in metrics :
          "${namespace}:${metric}" => {
            "namespace"   = namespace,
            "metric_name" = metric
          }
        })
      ]...)
    }
    if value.include_mean || length(value.explicit_bounds) > 0
  }
}

resource "aws_cloudwatch_metric_stream" "metric_stream" {
  name          = local.metric_stream_name
  role_arn      = var.metric_stream_role_arn
  firehose_arn  = aws_kinesis_firehose_delivery_stream.firehose_stream.arn
  output_format = "json"

  dynamic "include_filter" {
    for_each = local.all_namespaced_metrics
    content {
      namespace    = include_filter.key
      metric_names = include_filter.value
    }
  }

  # Here’s an example of what a statistics_configuration block may look like:
  #
  # statistics_configuration {
  #     additional_statistics = [
  #         "TC(0:5)",
  #         "TC(10000:)",
  #         "TC(1000:2500)",
  #         "TC(100:250)",
  #         "TC(10:25)",
  #         "TC(2500:5000)",
  #         "TC(250:500)",
  #         "TC(25:50)",
  #         "TC(5000:7500)",
  #         "TC(500:750)",
  #         "TC(50:75)",
  #         "TC(5:10)",
  #         "TC(7500:10000)",
  #         "TC(750:1000)",
  #         "TC(75:100)",
  #         "TC(:0)",
  #         "TM(0%:100%)",
  #     ]
  #
  #     include_metric {
  #         metric_name = "Latency"
  #         namespace   = "AWS/ApiGateway"
  #     }
  #     include_metric {
  #         metric_name = "OriginLatency"
  #         namespace   = "AWS/CloudFront"
  #     }
  # }
  dynamic "statistics_configuration" {
    for_each = local.statistics_configuration
    content {
      additional_statistics = statistics_configuration.value.additional_statistics

      dynamic "include_metric" {
        for_each = statistics_configuration.value.include_metric
        content {
          namespace   = include_metric.value.namespace
          metric_name = include_metric.value.metric_name
        }
      }
    }
  }
}
