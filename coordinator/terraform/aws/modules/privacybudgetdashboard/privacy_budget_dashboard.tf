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
resource "aws_cloudwatch_dashboard" "privacy_budget_dashboard" {
  dashboard_name = "${var.environment}-privacy_budget_dashboard"

  dashboard_body = jsonencode(
    {
      "start" : "-PT168H",
      "widgets" : [
        {
          "height" : 6,
          "width" : 12,
          "y" : 2,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              [{ "expression" : "FILL(m1, 0)/60", "label" : "Requests Count", "id" : "e1", "region" : "${var.region}", "stat" : "Sum", "period" : var.privacy_budget_dashboard_time_period_seconds, "yAxis" : "left" }],
              ["AWS/ApiGateway", "Count", "ApiId", "${var.privacy_budget_api_gateway_id}", { "id" : "m1", "visible" : false, "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "PBS Auth APIGateway Requests Count",
            "stat" : "Sum",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "yAxis" : {
              "left" : {
                "label" : "Count",
                "showUnits" : false
              },
              "right" : {
                "label" : "",
                "showUnits" : false
              }
            }
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 49,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/EC2", "CPUUtilization", { "visible" : false, "region" : "${var.region}" }],
              [".", ".", "AutoScalingGroupName", "${var.privacy_budget_autoscaling_group_name}", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "stat" : "Sum",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "title" : "CPU Utilization across PBS autoscaling group"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 2,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/ApiGateway", "5xx", "ApiId", "${var.privacy_budget_api_gateway_id}", { "region" : "${var.region}" }],
              [".", "4xx", ".", ".", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "PBS Auth APIGateway Errors",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum",
            "yAxis" : {
              "left" : {
                "showUnits" : false,
                "label" : "Count"
              },
              "right" : {
                "showUnits" : false
              }
            }
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 8,
          "x" : 6,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/ApiGateway", "Latency", "ApiId", "${var.privacy_budget_api_gateway_id}", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "PBS Auth API APIGateway Latency",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum",
            "yAxis" : {
              "left" : {
                "showUnits" : false,
                "label" : "Millisecond"
              },
              "right" : {
                "showUnits" : false
              }
            }
          }
        },
        {
          "height" : 1,
          "width" : 24,
          "y" : 83,
          "x" : 0,
          "type" : "text",
          "properties" : {
            "markdown" : "# PBS Transactions",
            "background" : "transparent"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 55,
          "x" : 5,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/EC2", "NetworkIn", "AutoScalingGroupName", "${var.privacy_budget_autoscaling_group_name}", { "region" : "${var.region}" }],
              [".", "NetworkOut", ".", ".", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "NetworkIn, NetworkOut across PBS autoscaling group",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 49,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/EC2", "EBSWriteBytes", "AutoScalingGroupName", "${var.privacy_budget_autoscaling_group_name}", { "region" : "${var.region}" }],
              [".", "EBSReadBytes", ".", ".", { "region" : "${var.region}" }],
              [".", "EBSReadOps", ".", ".", { "region" : "${var.region}" }],
              [".", "EBSWriteOps", ".", ".", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "Read/Write I/O across PBS autoscaling group",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 1,
          "width" : 24,
          "y" : 0,
          "x" : 0,
          "type" : "text",
          "properties" : {
            "markdown" : "# PBS Auth API \n",
            "background" : "transparent"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 85,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["${var.environment}-google-scp-pbs", "TotalRequest", "ReportingOrigin", "Operator", "ComponentName", "FrontEndService", "MethodName", "BeginTransaction", { "region" : "${var.region}" }],
              [".", "ServerErrors", ".", ".", ".", ".", ".", ".", { "region" : "${var.region}" }],
              [".", "TotalRequest", ".", ".", ".", ".", ".", "EndTransaction", { "region" : "${var.region}" }],
              [".", "ServerErrors", ".", ".", ".", ".", ".", ".", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "Operator Begin/End Transactions",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 91,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["${var.environment}-google-scp-pbs", "TotalRequest", "ReportingOrigin", "Operator", "ComponentName", "FrontEndService", "MethodName", "GetStatusTransaction", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "Operator GetStatusTransaction",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 85,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["${var.environment}-google-scp-pbs", "TotalRequest", "ReportingOrigin", "Operator", "ComponentName", "FrontEndService", "MethodName", "CommitTransaction", { "region" : "${var.region}" }],
              ["...", "NotifyTransaction", { "region" : "${var.region}" }],
              [".", "ServerErrors", ".", ".", ".", ".", ".", "PrepareTransaction", { "region" : "${var.region}" }],
              [".", "TotalRequest", ".", ".", ".", ".", ".", ".", { "region" : "${var.region}" }],
              [".", "ServerErrors", ".", ".", ".", ".", ".", "CommitTransaction", { "region" : "${var.region}" }],
              ["...", "NotifyTransaction", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "Operator Prepare/Commit/Notify",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 91,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["${var.environment}-google-scp-pbs", "TotalRequest", "ReportingOrigin", "Operator", "ComponentName", "FrontEndService", "MethodName", "AbortTransaction", { "region" : "${var.region}" }],
              [".", "ServerErrors", ".", ".", ".", ".", ".", ".", { "region" : "${var.region}" }],
              [".", "TotalRequest", ".", "Coordinator", ".", ".", ".", ".", { "region" : "${var.region}", "visible" : false }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "Operator Abort Transactions",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 1,
          "width" : 24,
          "y" : 48,
          "x" : 0,
          "type" : "text",
          "properties" : {
            "markdown" : "# PBS Resource Usage",
            "background" : "transparent"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 65,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/DynamoDB", "ProvisionedWriteCapacityUnits", "TableName", "${var.environment}-google-scp-pbs-budget-keys", { "region" : "${var.region}" }],
              [".", "ConsumedWriteCapacityUnits", ".", ".", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "title" : "pbs-budget-keys Write Capacity",
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 65,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/DynamoDB", "ConsumedReadCapacityUnits", "TableName", "${var.environment}-google-scp-pbs-budget-keys", { "region" : "${var.region}", "color" : "#ff7f0e" }],
              [".", "ProvisionedReadCapacityUnits", ".", ".", { "region" : "${var.region}", "color" : "#1f77b4" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "pbs-budget-keys Read Capacity",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 71,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/DynamoDB", "ProvisionedWriteCapacityUnits", "TableName", "${var.environment}-google-scp-pbs-reporting-origin-auth", { "region" : "${var.region}" }],
              [".", "ConsumedWriteCapacityUnits", ".", ".", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "pbs-reporting-origin-auth Write Capacity",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 71,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/DynamoDB", "ConsumedReadCapacityUnits", "TableName", "${var.environment}-google-scp-pbs-reporting-origin-auth", { "region" : "${var.region}", "color" : "#ff7f0e" }],
              [".", "ProvisionedReadCapacityUnits", ".", ".", { "region" : "${var.region}", "color" : "#1f77b4" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "pbs-reporting-origin-auth Read Capacity",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 1,
          "width" : 24,
          "y" : 104,
          "x" : 0,
          "type" : "text",
          "properties" : {
            "markdown" : "## PBS Authorization V2 DynamoDB table",
            "background" : "transparent"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 105,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/DynamoDB", "ProvisionedWriteCapacityUnits", "TableName", "${var.environment}-pbs-authorization-v2", { "region" : "${var.region}" }],
              [".", "ConsumedWriteCapacityUnits", ".", ".", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "pbs-authorization-v2 Write Capacity",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 105,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/DynamoDB", "ConsumedReadCapacityUnits", "TableName", "${var.environment}-pbs-authorization-v2", { "region" : "${var.region}", "color" : "#ff7f0e" }],
              [".", "ProvisionedReadCapacityUnits", ".", ".", { "region" : "${var.region}", "color" : "#1f77b4" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "pbs-authorization-v2 Read Capacity",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 77,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/DynamoDB", "ProvisionedReadCapacityUnits", "TableName", "${var.environment}-google-scp-pbs-partition-lock", { "region" : "${var.region}" }],
              [".", "ConsumedReadCapacityUnits", ".", ".", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "pbs-partition-lock Read Capacity",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 77,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/DynamoDB", "ConsumedWriteCapacityUnits", "TableName", "${var.environment}-google-scp-pbs-reporting-origin-auth", { "region" : "${var.region}", "color" : "#ff7f0e" }],
              [".", "ProvisionedWriteCapacityUnits", ".", ".", { "region" : "${var.region}", "color" : "#1f77b4" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "pbs-partition-lock Write Capacity",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 3,
          "width" : 24,
          "y" : 62,
          "x" : 0,
          "type" : "alarm",
          "properties" : {
            "title" : "PBS Dynamo DB Alarms",
            "alarms" : [
              "arn:aws:cloudwatch:${var.region}:${var.account_id}:alarm:InfoDynamodbBudgetKeyTableWriteCapacityRatio${var.custom_alarm_label}",
              "arn:aws:cloudwatch:${var.region}:${var.account_id}:alarm:InfoDynamodbBudgetKeyTableReadCapacityRatio${var.custom_alarm_label}",
              "arn:aws:cloudwatch:${var.region}:${var.account_id}:alarm:InfoDynamodbReportingOriginTableWriteCapacityRatio${var.custom_alarm_label}",
              "arn:aws:cloudwatch:${var.region}:${var.account_id}:alarm:InfoDynamodbReportingOriginTableReadCapacityRatio${var.custom_alarm_label}",
              "arn:aws:cloudwatch:${var.region}:${var.account_id}:alarm:InfoDynamodbPBSAuthorizationV2TableWriteCapacityRatio${var.custom_alarm_label}",
              "arn:aws:cloudwatch:${var.region}:${var.account_id}:alarm:InfoDynamodbPBSAuthorizationV2TableReadCapacityRatio${var.custom_alarm_label}",
              "arn:aws:cloudwatch:${var.region}:${var.account_id}:alarm:InfoDynamodbPartitionLockTableReadCapacityRatio${var.custom_alarm_label}",
              "arn:aws:cloudwatch:${var.region}:${var.account_id}:alarm:InfoDynamodbPartitionLockTableWriteCapacityRatio${var.custom_alarm_label}"
            ]
          }
        },
        {
          "height" : 1,
          "width" : 24,
          "y" : 61,
          "x" : 0,
          "type" : "text",
          "properties" : {
            "markdown" : "# PBS Dynamo DB",
            "background" : "transparent"
          }
        },
        {
          "height" : 2,
          "width" : 24,
          "y" : 34,
          "x" : 0,
          "type" : "alarm",
          "properties" : {
            "title" : "PBS ELB Errors alarms",
            "alarms" : [
              "arn:aws:cloudwatch:${var.region}:${var.account_id}:alarm:WarningELB4xxErrorRatioHigh${var.custom_alarm_label}",
            ]
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 36,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/ApplicationELB", "RequestCount", "LoadBalancer", "${var.privacy_budget_load_balancer_id}"]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "ELB Request Count",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum",
            "yAxis" : {
              "left" : {
                "label" : "Count",
                "showUnits" : false
              }
            }
          }
        },
        {
          "height" : 1,
          "width" : 24,
          "y" : 14,
          "x" : 0,
          "type" : "text",
          "properties" : {
            "markdown" : "## Lambda",
            "background" : "transparent"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 98,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["${var.environment}-google-scp-pbs", "TotalRequest", "ReportingOrigin", "Coordinator", "ComponentName", "FrontEndService", "MethodName", "AbortTransaction", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "Coordinator Abort Transactions",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 98,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["${var.environment}-google-scp-pbs", "TotalRequest", "ReportingOrigin", "Coordinator", "ComponentName", "FrontEndService", "MethodName", "GetStatusTransaction", { "region" : "${var.region}" }],
              [".", "ClientErrors", ".", ".", ".", ".", ".", ".", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "Coordinator GetStatusTransaction",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 1,
          "width" : 24,
          "y" : 84,
          "x" : 0,
          "type" : "text",
          "properties" : {
            "markdown" : "## Operator",
            "background" : "transparent"
          }
        },
        {
          "height" : 1,
          "width" : 24,
          "y" : 97,
          "x" : 0,
          "type" : "text",
          "properties" : {
            "markdown" : "## Coordinator",
            "background" : "transparent"
          }
        },
        {
          "height" : 1,
          "width" : 24,
          "y" : 33,
          "x" : 0,
          "type" : "text",
          "properties" : {
            "markdown" : "# Elastic Load Balancer (ELB)",
            "background" : "transparent"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 15,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/Lambda", "Errors", "FunctionName", "${var.environment}-google-scp-pbs-auth-lambda", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "pbs-auth-lambda Errors",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 27,
          "x" : 6,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/Lambda", "Throttles", "FunctionName", "${var.environment}-google-scp-pbs-auth-lambda", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "pbs-auth-lambda Throttles",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 15,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/Lambda", "ConcurrentExecutions", "FunctionName", "${var.environment}-google-scp-pbs-auth-lambda", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "pbs-auth-lambda ConcurrentExecutions",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 21,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "view" : "timeSeries",
            "stacked" : false,
            "metrics" : [
              ["AWS/Lambda", "Duration", "FunctionName", "${var.environment}-google-scp-pbs-auth-lambda"]
            ],
            "region" : "${var.region}",
            "title" : "pbs-auth-lambda Duration"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 21,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "metrics" : [
              ["AWS/Lambda", "Invocations", "FunctionName", "${var.environment}-google-scp-pbs-auth-lambda", { "region" : "${var.region}" }]
            ],
            "view" : "timeSeries",
            "stacked" : false,
            "region" : "${var.region}",
            "title" : "pbs-auth-lambda Invocations",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 1,
          "width" : 24,
          "y" : 1,
          "x" : 0,
          "type" : "text",
          "properties" : {
            "markdown" : "## API Gateway",
            "background" : "transparent"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 36,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "view" : "timeSeries",
            "stacked" : false,
            "metrics" : [
              ["AWS/ApplicationELB", "HTTPCode_ELB_4XX_Count", "LoadBalancer", "${var.privacy_budget_load_balancer_id}"],
              [".", "HTTPCode_ELB_5XX_Count", ".", "."]
            ],
            "region" : "${var.region}",
            "title" : "ELB Error Count",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 42,
          "x" : 0,
          "type" : "metric",
          "properties" : {
            "view" : "timeSeries",
            "stacked" : false,
            "metrics" : [
              ["AWS/ApplicationELB", "HTTPCode_Target_4XX_Count", "LoadBalancer", "${var.privacy_budget_load_balancer_id}"],
              [".", "HTTPCode_Target_2XX_Count", ".", "."]
            ],
            "region" : "${var.region}",
            "title" : "Target Response Codes",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Sum"
          }
        },
        {
          "height" : 6,
          "width" : 12,
          "y" : 42,
          "x" : 12,
          "type" : "metric",
          "properties" : {
            "view" : "timeSeries",
            "stacked" : false,
            "metrics" : [
              ["AWS/ApplicationELB", "TargetResponseTime", "LoadBalancer", "${var.privacy_budget_load_balancer_id}"]
            ],
            "region" : "${var.region}",
            "title" : "Target Response Time",
            "period" : var.privacy_budget_dashboard_time_period_seconds,
            "stat" : "Average"
          }
        }
      ]
    }
  )
}
