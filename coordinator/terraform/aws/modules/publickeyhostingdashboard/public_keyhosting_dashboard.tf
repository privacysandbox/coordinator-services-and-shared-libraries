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
# "arn:aws:cloudwatch:${var.primary_region}:${var.account_id}:alarm:${var.environment}_${var.primary_region}_get_public_key_api_api_gateway_max_latency_alarm",
# "arn:aws:cloudwatch:${var.primary_region}:${var.account_id}:alarm:${var.environment}_${var.primary_region}_get_public_key_lambda_max_duration_alarm",
#"arn:aws:cloudwatch:${var.primary_region}:${var.account_id}:alarm:${var.environment}_${var.primary_region}_get_public_key_api_api_gateway_5xx_alarm",
#"arn:aws:cloudwatch:${var.primary_region}:${var.account_id}:alarm:${var.environment}_${var.primary_region}_get_public_key_lambda_error_alarm",
#"arn:aws:cloudwatch:${var.primary_region}:${var.account_id}:alarm:${var.environment}_${var.primary_region}_get_public_key_lambda_error_log_alarm"
#"arn:aws:cloudwatch:us-east-1:${var.account_id}:alarm:${var.environment}_get_public_key_cloudfront_origin_latency_alarm",
#"arn:aws:cloudwatch:us-east-1:${var.account_id}:alarm:${var.environment}_get_public_key_cloudfront_cache_hit_alarm",
#"arn:aws:cloudwatch:us-east-1:${var.account_id}:alarm:${var.environment}_get_public_key_cloudfront_5xx_alarm"

resource "aws_cloudwatch_dashboard" "public_key_dashboard" {
  dashboard_name = "${var.environment}_public_key_hosting_dashboard"
  dashboard_body = jsonencode(
    {
      "widgets" : [
        {
          "height" : 2,
          "width" : 24,
          "y" : 8,
          "x" : 0,
          "type" : "alarm",
          "properties" : {
            "title" : "API Gateway Alarms (${var.primary_region})",
            "alarms" : [
              "arn:aws:cloudwatch:${var.primary_region}:${var.account_id}:alarm:${var.environment}_${var.primary_region}_get_public_key_api_api_gateway_max_latency_alarm",
              "arn:aws:cloudwatch:${var.primary_region}:${var.account_id}:alarm:${var.environment}_${var.primary_region}_get_public_key_lambda_max_duration_alarm",
              "arn:aws:cloudwatch:${var.primary_region}:${var.account_id}:alarm:${var.environment}_${var.primary_region}_get_public_key_api_api_gateway_5xx_alarm",
              "arn:aws:cloudwatch:${var.primary_region}:${var.account_id}:alarm:${var.environment}_${var.primary_region}_get_public_key_lambda_error_alarm",
              "arn:aws:cloudwatch:${var.primary_region}:${var.account_id}:alarm:${var.environment}_${var.primary_region}_get_public_key_lambda_error_log_alarm"
            ]
          }
        },
        {
          "height" : 2,
          "width" : 24,
          "y" : 0,
          "x" : 0,
          "type" : "alarm",
          "properties" : {
            "title" : "Cloudfront Alarms",
            "alarms" : [
              "arn:aws:cloudwatch:us-east-1:${var.account_id}:alarm:${var.environment}_get_public_key_cloudfront_origin_latency_alarm",
              "arn:aws:cloudwatch:us-east-1:${var.account_id}:alarm:${var.environment}_get_public_key_cloudfront_cache_hit_alarm",
              "arn:aws:cloudwatch:us-east-1:${var.account_id}:alarm:${var.environment}_get_public_key_cloudfront_5xx_alarm"
            ]
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 2,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "view" : "timeSeries",
            "stacked" : false,
            "metrics" : [
              [
                "AWS/CloudFront",
                "Requests",
                "Region",
                "Global",
                "DistributionId",
                "${var.cloudfront_id}",
                {
                  "region" : "us-east-1"
                }
              ]
            ],
            "region" : "${var.primary_region}",
            "title" : "CloudFront Requests",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 2,
          "x" : 6,
          "type" : "metric",
          "properties" : {
            "view" : "timeSeries",
            "stacked" : false,
            "metrics" : [
              [
                "AWS/CloudFront",
                "CacheHitRate",
                "Region",
                "Global",
                "DistributionId",
                "${var.cloudfront_id}",
                {
                  "region" : "us-east-1"
                }
              ]
            ],
            "region" : "${var.primary_region}",
            "title" : "CloudFront CacheHit Rate",
            "period" : var.get_public_key_dashboard_time_period_seconds
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 2,
          "x" : 18,
          "type" : "metric",
          "properties" : {
            "view" : "timeSeries",
            "stacked" : false,
            "metrics" : [
              [
                "AWS/CloudFront",
                "OriginLatency",
                "Region",
                "Global",
                "DistributionId",
                "${var.cloudfront_id}",
                {
                  "region" : "us-east-1",
                  "stat" : "p50",
                  "label" : "p50"
                }
              ],
              [
                "...",
                {
                  "region" : "us-east-1",
                  "stat" : "p90",
                  "label" : "p90"
                }
              ],
              [
                "...",
                {
                  "region" : "us-east-1",
                  "label" : "p99"
                }
              ]
            ],
            "region" : "${var.primary_region}",
            "title" : "CloudFront Origin Latency",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "p99"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 2,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/CloudFront",
                "5xxErrorRate",
                "Region",
                "Global",
                "DistributionId",
                "${var.cloudfront_id}",
                {
                  "label" : "5xx"
                }
              ],
              [
                ".",
                "4xxErrorRate",
                ".",
                ".",
                ".",
                ".",
                {
                  "label" : "4xx"
                }
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.primary_region}",
            "title" : "CloudFront Error Rates",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Average"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 10,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/ApiGateway",
                "Count",
                "ApiId",
                "${var.get_public_key_primary_api_gateway_id}"
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.primary_region}",
            "title" : "API Gateway Requests (${var.primary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 10,
          "x" : 6,
          "type" : "metric",
          "properties" : {
            "view" : "timeSeries",
            "stacked" : false,
            "metrics" : [
              [
                "AWS/ApiGateway",
                "Latency",
                "ApiId",
                "${var.get_public_key_primary_api_gateway_id}",
                {
                  "stat" : "p50",
                  "label" : "p50"
                }
              ],
              [
                "...",
                {
                  "stat" : "p90",
                  "label" : "p90"
                }
              ],
              [
                "...",
                {
                  "label" : "p99"
                }
              ]
            ],
            "region" : "${var.primary_region}",
            "title" : "API Gateway Latency (${var.primary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "p99"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 10,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/ApiGateway",
                "4xx",
                "ApiId",
                "${var.get_public_key_primary_api_gateway_id}"
              ],
              [
                ".",
                "5xx",
                ".",
                "."
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.primary_region}",
            "title" : "API Gateway Errors (${var.primary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 26,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/Lambda",
                "Duration",
                "FunctionName",
                "${var.environment}_${var.primary_region}_get_public_key_lambda",
                {
                  "stat" : "p50",
                  "label" : "p50"
                }
              ],
              [
                "...",
                {
                  "stat" : "p90",
                  "label" : "p90"
                }
              ],
              [
                "...",
                {
                  "label" : "p99"
                }
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.primary_region}",
            "title" : "Lambda Duration (${var.primary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "p99"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 26,
          "x" : 6,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/Lambda",
                "ProvisionedConcurrencyUtilization",
                "FunctionName",
                "${var.environment}_${var.primary_region}_get_public_key_lambda",
                "Resource",
                "${var.environment}_${var.primary_region}_get_public_key_lambda:${var.get_public_key_primary_get_public_key_lambda_version}",
                {
                  "label" : "Utilization Rate"
                }
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.primary_region}",
            "title" : "Lambda Provisioned Concurrency Utilization (${var.primary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Average"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 26,
          "x" : 18,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/Lambda",
                "Throttles",
                "FunctionName",
                "${var.environment}_${var.primary_region}_get_public_key_lambda"
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.primary_region}",
            "title" : "Lambda Throttles (${var.primary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 26,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/Lambda",
                "Invocations",
                "FunctionName",
                "${var.environment}_${var.primary_region}_get_public_key_lambda"
              ],
              [
                ".",
                "ProvisionedConcurrencyInvocations",
                ".",
                ".",
                {
                  "label" : "ProvisionedConcurrency Invocations"
                }
              ],
              [
                ".",
                "ProvisionedConcurrencySpilloverInvocations",
                ".",
                ".",
                {
                  "label" : "ProvisionedConcurrency Spillover Invocations"
                }
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.primary_region}",
            "title" : "Lambda Invocations (${var.primary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 53,
          "x" : 6,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/DynamoDB",
                "ProvisionedReadCapacityUnits",
                "TableName",
                "${var.environment}_keydb"
              ],
              [
                ".",
                "ConsumedReadCapacityUnits",
                ".",
                "."
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.primary_region}",
            "title" : "DynamoDB Read Capacity (${var.primary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Average"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 53,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/DynamoDB",
                "SuccessfulRequestLatency",
                "TableName",
                "${var.environment}_keydb",
                "Operation",
                "Scan",
                {
                  "stat" : "p50",
                  "label" : "p50"
                }
              ],
              [
                "...",
                {
                  "stat" : "p90",
                  "label" : "p90"
                }
              ],
              [
                "...",
                {
                  "label" : "p99"
                }
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.primary_region}",
            "title" : "DynamoDB Request Latency (${var.primary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "p99"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 53,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/DynamoDB",
                "ReturnedItemCount",
                "TableName",
                "${var.environment}_keydb",
                "Operation",
                "Scan"
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.primary_region}",
            "title" : "DynamoDB Returned Item Count (${var.primary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Average"
          }
        },
        {
          "height" : 2,
          "width" : 24,
          "y" : 16,
          "x" : 0,
          "type" : "alarm",
          "properties" : {
            "title" : "API Gateway Alarms (${var.secondary_region})",
            "alarms" : [
              "arn:aws:cloudwatch:${var.secondary_region}:${var.account_id}:alarm:${var.environment}_${var.secondary_region}_get_public_key_api_api_gateway_max_latency_alarm",
              "arn:aws:cloudwatch:${var.secondary_region}:${var.account_id}:alarm:${var.environment}_${var.secondary_region}_get_public_key_lambda_error_alarm",
              "arn:aws:cloudwatch:${var.secondary_region}:${var.account_id}:alarm:${var.environment}_${var.secondary_region}_get_public_key_lambda_error_log_alarm",
              "arn:aws:cloudwatch:${var.secondary_region}:${var.account_id}:alarm:${var.environment}_${var.secondary_region}_get_public_key_api_api_gateway_5xx_alarm",
              "arn:aws:cloudwatch:${var.secondary_region}:${var.account_id}:alarm:${var.environment}_${var.secondary_region}_get_public_key_lambda_max_duration_alarm"
            ]
          }
        },
        {
          "height" : 2,
          "width" : 24,
          "y" : 38,
          "x" : 0,
          "type" : "alarm",
          "properties" : {
            "title" : "Lambda Alarms (${var.secondary_region})",
            "alarms" : [
              "arn:aws:cloudwatch:${var.secondary_region}:${var.account_id}:alarm:${var.environment}_${var.secondary_region}_get_public_key_lambda_error_alarm",
              "arn:aws:cloudwatch:${var.secondary_region}:${var.account_id}:alarm:${var.environment}_${var.secondary_region}_get_public_key_lambda_error_log_alarm",
              "arn:aws:cloudwatch:${var.secondary_region}:${var.account_id}:alarm:${var.environment}_${var.secondary_region}_get_public_key_lambda_max_duration_alarm"
            ]
          }
        },
        {
          "height" : 2,
          "width" : 24,
          "y" : 24,
          "x" : 0,
          "type" : "alarm",
          "properties" : {
            "title" : "Lambda Alarms (${var.primary_region})",
            "alarms" : [
              "arn:aws:cloudwatch:${var.primary_region}:${var.account_id}:alarm:${var.environment}_${var.primary_region}_get_public_key_lambda_max_duration_alarm",
              "arn:aws:cloudwatch:${var.primary_region}:${var.account_id}:alarm:${var.environment}_${var.primary_region}_get_public_key_lambda_error_alarm",
              "arn:aws:cloudwatch:${var.primary_region}:${var.account_id}:alarm:${var.environment}_${var.primary_region}_get_public_key_lambda_error_log_alarm"
            ]
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 18,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "view" : "timeSeries",
            "stacked" : false,
            "metrics" : [
              [
                "AWS/ApiGateway",
                "Count",
                "ApiId",
                "${var.get_public_key_secondary_api_gateway_id}",
                {
                  "region" : "${var.secondary_region}"
                }
              ]
            ],
            "region" : "${var.primary_region}",
            "title" : "API Gateway Requests (${var.secondary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 18,
          "x" : 6,
          "type" : "metric",
          "properties" : {
            "view" : "timeSeries",
            "stacked" : false,
            "metrics" : [
              [
                "AWS/ApiGateway",
                "Latency",
                "ApiId",
                "${var.get_public_key_secondary_api_gateway_id}",
                {
                  "region" : "${var.secondary_region}",
                  "stat" : "p50",
                  "label" : "p50"
                }
              ],
              [
                "...",
                {
                  "region" : "${var.secondary_region}",
                  "stat" : "p90",
                  "label" : "p90"
                }
              ],
              [
                "...",
                {
                  "region" : "${var.secondary_region}",
                  "label" : "p99"
                }
              ]
            ],
            "region" : "${var.primary_region}",
            "title" : "API Gateway Latency (${var.secondary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "p99"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 18,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "view" : "timeSeries",
            "stacked" : false,
            "metrics" : [
              [
                "AWS/ApiGateway",
                "4xx",
                "ApiId",
                "${var.get_public_key_secondary_api_gateway_id}",
                {
                  "region" : "${var.secondary_region}"
                }
              ],
              [
                ".",
                "5xx",
                ".",
                ".",
                {
                  "region" : "${var.secondary_region}"
                }
              ]
            ],
            "region" : "${var.primary_region}",
            "title" : "API Gateway Errors (${var.secondary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 40,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "view" : "timeSeries",
            "stacked" : false,
            "metrics" : [
              [
                "AWS/Lambda",
                "Duration",
                "FunctionName",
                "${var.environment}_${var.secondary_region}_get_public_key_lambda",
                {
                  "region" : "${var.secondary_region}",
                  "stat" : "p50",
                  "label" : "p50"
                }
              ],
              [
                "...",
                {
                  "region" : "${var.secondary_region}",
                  "stat" : "p90",
                  "label" : "p90"
                }
              ],
              [
                "...",
                {
                  "region" : "${var.secondary_region}",
                  "label" : "p99"
                }
              ]
            ],
            "region" : "${var.primary_region}",
            "title" : "Lambda Duration (${var.secondary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "p99"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 40,
          "x" : 6,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/Lambda",
                "ProvisionedConcurrencyUtilization",
                "FunctionName",
                "${var.environment}_${var.secondary_region}_get_public_key_lambda",
                "Resource",
                "${var.environment}_${var.secondary_region}_get_public_key_lambda:${var.get_public_key_secondary_get_public_key_lambda_version}",
                {
                  "region" : "${var.secondary_region}",
                  "label" : "Utilization Rate"
                }
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.primary_region}",
            "title" : "Lambda Provisioned Concurrency Utilization (${var.secondary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Average"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 40,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/Lambda",
                "Invocations",
                "FunctionName",
                "${var.environment}_${var.secondary_region}_get_public_key_lambda",
                {
                  "region" : "${var.secondary_region}"
                }
              ],
              [
                ".",
                "ProvisionedConcurrencyInvocations",
                ".",
                ".",
                {
                  "region" : "${var.secondary_region}",
                  "label" : "ProvisionedConcurrency Invocations"
                }
              ],
              [
                ".",
                "ProvisionedConcurrencySpilloverInvocations",
                ".",
                ".",
                {
                  "region" : "${var.secondary_region}",
                  "label" : "ProvisionedConcurrency Spillover Invocations"
                }
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.primary_region}",
            "title" : "Lambda Invocations (${var.secondary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 40,
          "x" : 18,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/Lambda",
                "Throttles",
                "FunctionName",
                "${var.environment}_${var.secondary_region}_get_public_key_lambda",
                {
                  "region" : "${var.secondary_region}"
                }
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.primary_region}",
            "title" : "Lambda Throttles (${var.secondary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 32,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/Lambda",
                "Errors",
                "FunctionName",
                "${var.environment}_${var.primary_region}_get_public_key_lambda"
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.primary_region}",
            "title" : "Lambda Errors (${var.primary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 46,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/Lambda",
                "Errors",
                "FunctionName",
                "${var.environment}_${var.secondary_region}_get_public_key_lambda",
                {
                  "region" : "${var.secondary_region}"
                }
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.primary_region}",
            "title" : "Lambda Errors (${var.secondary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 1,
          "width" : 24,
          "y" : 52,
          "x" : 0,
          "type" : "text",
          "properties" : {
            "markdown" : "#### DynamoDB Metrics (${var.primary_region})"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 60,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "view" : "timeSeries",
            "stacked" : false,
            "metrics" : [
              [
                "AWS/DynamoDB",
                "ReturnedItemCount",
                "TableName",
                "${var.environment}_keydb",
                "Operation",
                "Scan",
                {
                  "region" : "${var.secondary_region}"
                }
              ]
            ],
            "region" : "${var.primary_region}",
            "title" : "DynamoDB Returned Item Count (${var.secondary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 60,
          "x" : 6,
          "type" : "metric",
          "properties" : {
            "view" : "timeSeries",
            "stacked" : false,
            "metrics" : [
              [
                "AWS/DynamoDB",
                "ProvisionedReadCapacityUnits",
                "TableName",
                "${var.environment}_keydb",
                {
                  "region" : "${var.secondary_region}"
                }
              ],
              [
                ".",
                "ConsumedReadCapacityUnits",
                ".",
                ".",
                {
                  "region" : "${var.secondary_region}"
                }
              ]
            ],
            "region" : "${var.primary_region}",
            "title" : "DynamoDB Read Capacity (${var.secondary_region})"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 60,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "view" : "timeSeries",
            "stacked" : false,
            "metrics" : [
              [
                "AWS/DynamoDB",
                "SuccessfulRequestLatency",
                "TableName",
                "${var.environment}_keydb",
                "Operation",
                "Scan",
                {
                  "region" : "${var.secondary_region}",
                  "stat" : "p50",
                  "label" : "p50"
                }
              ],
              [
                "...",
                {
                  "region" : "${var.secondary_region}",
                  "stat" : "p90",
                  "label" : "p90"
                }
              ],
              [
                "...",
                {
                  "region" : "${var.secondary_region}",
                  "label" : "p99"
                }
              ]
            ],
            "region" : "${var.primary_region}",
            "title" : "DynamoDB Request Latency (${var.secondary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "p99"
          }
        },
        {
          "height" : 1,
          "width" : 24,
          "y" : 59,
          "x" : 0,
          "type" : "text",
          "properties" : {
            "markdown" : "#### DynamoDB Metrics (${var.secondary_region})"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 10,
          "x" : 18,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                {
                  "expression" : "100*(m1+m2/m3)",
                  "label" : "Percentage",
                  "id" : "e1",
                  "region" : "${var.primary_region}"
                }
              ],
              [
                "AWS/ApiGateway",
                "4xx",
                "ApiId",
                "${var.get_public_key_primary_api_gateway_id}",
                {
                  "id" : "m1",
                  "visible" : false
                }
              ],
              [
                ".",
                "5xx",
                ".",
                ".",
                {
                  "id" : "m2",
                  "visible" : false
                }
              ],
              [
                ".",
                "Count",
                ".",
                ".",
                {
                  "id" : "m3",
                  "visible" : false
                }
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.primary_region}",
            "title" : "API Gateway Error Percentage (us-west-2)",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Average"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 18,
          "x" : 18,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                {
                  "expression" : "100*(m1+m2/m3)",
                  "label" : "Percentage",
                  "id" : "e1"
                }
              ],
              [
                "AWS/ApiGateway",
                "4xx",
                "ApiId",
                "${var.get_public_key_secondary_api_gateway_id}",
                {
                  "region" : "${var.secondary_region}",
                  "id" : "m1",
                  "visible" : false
                }
              ],
              [
                ".",
                "5xx",
                ".",
                ".",
                {
                  "region" : "${var.secondary_region}",
                  "id" : "m2",
                  "visible" : false
                }
              ],
              [
                ".",
                "Count",
                ".",
                ".",
                {
                  "region" : "${var.secondary_region}",
                  "id" : "m3",
                  "visible" : false
                }
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.primary_region}",
            "title" : "API Gateway Error Percentage (${var.secondary_region})",
            "period" : var.get_public_key_dashboard_time_period_seconds,
            "stat" : "Average"
          }
        }
      ]
    }
  )
}
