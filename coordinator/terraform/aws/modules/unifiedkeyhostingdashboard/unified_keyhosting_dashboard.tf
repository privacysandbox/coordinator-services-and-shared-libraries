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

resource "aws_cloudwatch_dashboard" "unified_keyhosting_dashboard" {
  dashboard_name = "${var.environment}-unified_keyhosting_dashboard"

  dashboard_body = jsonencode(
    {
      "widgets" : [
        {
          "height" : 2,
          "width" : 24,
          "y" : 0,
          "x" : 0,
          "type" : "alarm",
          "properties" : {
            "title" : "API Gateway Alarms (Unified Keyhosting - ${var.region})",
            "alarms" : [
              "arn:aws:cloudwatch:${var.region}:${var.account_id}:alarm:${var.environment}_${var.region}_unified_key_hosting_api_gateway_max_latency_alarm",
              "arn:aws:cloudwatch:${var.region}:${var.account_id}:alarm:${var.environment}_${var.region}_unified_key_hosting_api_gateway_5xx_alarm",
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
            "metrics" : [
              [
                "AWS/ApiGateway",
                "Count",
                "ApiId",
                "${var.unified_keyhosting_api_gateway_id}"
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "API Gateway Requests (Unified Keyhosting - ${var.region})",
            "period" : var.unified_keyhosting_dashboard_time_period_seconds,
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
                "AWS/ApiGateway",
                "Latency",
                "ApiId",
                "${var.unified_keyhosting_api_gateway_id}",
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
            "region" : "${var.region}",
            "title" : "API Gateway Latency (Unified Keyhosting - ${var.region})",
            "period" : var.unified_keyhosting_dashboard_time_period_seconds,
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
                "AWS/ApiGateway",
                "4xx",
                "ApiId",
                "${var.unified_keyhosting_api_gateway_id}"
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
            "region" : "${var.region}",
            "title" : "API Gateway Errors (Unified Keyhosting - ${var.region})",
            "period" : var.unified_keyhosting_dashboard_time_period_seconds,
            "stat" : "Sum"
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
                "AWS/Lambda",
                "Duration",
                "FunctionName",
                "${var.environment}_${var.region}_get_encryption_key_lambda",
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
            "region" : "${var.region}",
            "title" : "Lambda Duration (GetEncryptionKey - ${var.region})",
            "period" : var.unified_keyhosting_dashboard_time_period_seconds,
            "stat" : "p99"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 10,
          "x" : 6,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/Lambda",
                "ProvisionedConcurrencyUtilization",
                "FunctionName",
                "${var.environment}_${var.region}_get_encryption_key_lambda",
                "Resource",
                "${var.environment}_${var.region}_get_encryption_key_lambda:${var.get_encryption_key_lambda_version}",
                {
                  "label" : "Utilization Rate"
                }
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "Lambda Provisioned Concurrency Utilization (GetEncryptionKey - ${var.region})",
            "period" : var.unified_keyhosting_dashboard_time_period_seconds,
            "stat" : "Average"
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
                "AWS/Lambda",
                "Throttles",
                "FunctionName",
                "${var.environment}_${var.region}_get_encryption_key_lambda"
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "Lambda Throttles (GetEncryptionKey - ${var.region})",
            "period" : var.unified_keyhosting_dashboard_time_period_seconds,
            "stat" : "Sum"
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
                "AWS/Lambda",
                "Invocations",
                "FunctionName",
                "${var.environment}_${var.region}_get_encryption_key_lambda"
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
            "region" : "${var.region}",
            "title" : "Lambda Invocations (GetEncryptionKey - ${var.region})",
            "period" : var.unified_keyhosting_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 29,
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
            "region" : "${var.region}",
            "title" : "DynamoDB Read Capacity (${var.environment}_keydb - ${var.region})",
            "period" : var.unified_keyhosting_dashboard_time_period_seconds,
            "stat" : "Average"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 23,
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
            "region" : "${var.region}",
            "title" : "DynamoDB Scan Request Latency (${var.environment}_keydb - ${var.region})",
            "period" : var.unified_keyhosting_dashboard_time_period_seconds,
            "stat" : "p99"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 23,
          "x" : 6,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/DynamoDB",
                "SuccessfulRequestLatency",
                "TableName",
                "${var.environment}_keydb",
                "Operation",
                "GetItem",
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
            "region" : "${var.region}",
            "title" : "DynamoDB GetItem Request Latency (${var.environment}_keydb - ${var.region})",
            "period" : var.unified_keyhosting_dashboard_time_period_seconds,
            "stat" : "p99"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 23,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/DynamoDB",
                "SuccessfulRequestLatency",
                "TableName",
                "${var.environment}_keydb",
                "Operation",
                "PutItem",
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
            "region" : "${var.region}",
            "title" : "DynamoDB PutItem Request Latency (${var.environment}_keydb - ${var.region})",
            "period" : var.unified_keyhosting_dashboard_time_period_seconds,
            "stat" : "p99"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 29,
          "x" : 0,
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
            "region" : "${var.region}",
            "title" : "DynamoDB Scan Returned Item Count (${var.environment}_keydb - ${var.region})",
            "period" : var.unified_keyhosting_dashboard_time_period_seconds,
            "stat" : "Average"
          }
        },
        {
          "height" : 2,
          "width" : 24,
          "y" : 8,
          "x" : 0,
          "type" : "alarm",
          "properties" : {
            "title" : "Lambda Alarms (GetEncryptionKey - ${var.region})",
            "alarms" : [
              "arn:aws:cloudwatch:${var.region}:${var.account_id}:alarm:${var.environment}_${var.region}_get_encryption_key_lambda_max_duration_alarm",
              "arn:aws:cloudwatch:${var.region}:${var.account_id}:alarm:${var.environment}_${var.region}_get_encryption_key_lambda_error_alarm",
              "arn:aws:cloudwatch:${var.region}:${var.account_id}:alarm:${var.environment}_${var.region}_get_encryption_key_lambda_error_log_alarm"
            ]
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 16,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                "AWS/Lambda",
                "Errors",
                "FunctionName",
                "${var.environment}_${var.region}_get_encryption_key_lambda"
              ]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "Lambda Errors (GetEncryptionKey - ${var.region})",
            "period" : var.unified_keyhosting_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 1,
          "width" : 24,
          "y" : 22,
          "x" : 0,
          "type" : "text",
          "properties" : {
            "markdown" : "#### DynamoDB Metrics (${var.environment}_keydb - ${var.region})"
          }
        },
        {
          "height" : 6,
          "width" : 6,
          "y" : 2,
          "x" : 18,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [
                {
                  "expression" : "100*(m1+m2/m3)",
                  "label" : "Percentage",
                  "id" : "e1",
                  "region" : "${var.region}"
                }
              ],
              [
                "AWS/ApiGateway",
                "4xx",
                "ApiId",
                "${var.unified_keyhosting_api_gateway_id}",
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
            "region" : "${var.region}",
            "title" : "API Gateway Error Percentage (Unified Keyhosting - ${var.region})",
            "period" : var.unified_keyhosting_dashboard_time_period_seconds,
            "stat" : "Average"
          }
        }
      ]
    }
  )
}
