# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
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
  custom_metric_namespace = "${var.environment}/pbs/LogErrors"
}

##################
# Custom Metrics #
##################

resource "aws_cloudwatch_log_metric_filter" "error_level_metric_filter_log_corrupted" {
  name           = "metric_filter_log_corrupted"
  pattern        = "Log is corrupted"
  log_group_name = var.pbs_cloudwatch_log_group_name

  metric_transformation {
    name      = "metric_filter_log_corrupted"
    namespace = local.custom_metric_namespace
    value     = "1"
  }
}

resource "aws_cloudwatch_log_metric_filter" "error_level_metric_filter_missing_transaction" {
  name           = "metric_filter_missing_transaction"
  pattern        = "Cannot find the transaction with id"
  log_group_name = var.pbs_cloudwatch_log_group_name

  metric_transformation {
    name      = "metric_filter_missing_transaction"
    namespace = local.custom_metric_namespace
    value     = "1"
  }
}

resource "aws_cloudwatch_log_metric_filter" "error_level_metric_filter_checkpointing" {
  name           = "metric_filter_checkpointing"
  pattern        = "Checkpointing failed"
  log_group_name = var.pbs_cloudwatch_log_group_name

  metric_transformation {
    name      = "metric_filter_checkpointing"
    namespace = local.custom_metric_namespace
    value     = "1"
  }
}

resource "aws_cloudwatch_log_metric_filter" "error_level_metric_filter_database_read" {
  name           = "metric_filter_database_read"
  pattern        = "Failed to read lease from the database"
  log_group_name = var.pbs_cloudwatch_log_group_name

  metric_transformation {
    name      = "metric_filter_database_read"
    namespace = local.custom_metric_namespace
    value     = "1"
  }
}

resource "aws_cloudwatch_log_metric_filter" "error_level_metric_filter_database_update" {
  name           = "metric_filter_database_update"
  pattern        = "Failed to update lease from the database"
  log_group_name = var.pbs_cloudwatch_log_group_name

  metric_transformation {
    name      = "metric_filter_database_update"
    namespace = local.custom_metric_namespace
    value     = "1"
  }
}

resource "aws_cloudwatch_log_metric_filter" "error_level_metric_filter_missing_component_id" {
  name           = "metric_filter_missing_component_id"
  pattern        = "Cannot find the component with id"
  log_group_name = var.pbs_cloudwatch_log_group_name

  metric_transformation {
    name      = "metric_filter_missing_component_id"
    namespace = local.custom_metric_namespace
    value     = "1"
  }
}

resource "aws_cloudwatch_log_metric_filter" "error_level_metric_handle_journal" {
  name           = "metric_filter_handle_journal"
  pattern        = "Cannot handle the journal log with id"
  log_group_name = var.pbs_cloudwatch_log_group_name

  metric_transformation {
    name      = "metric_handle_journal"
    namespace = local.custom_metric_namespace
    value     = "1"
  }
}

##########
# Alarms #
##########

# Alarm on the count of logs containing "ERROR" (metric defined in this module).
resource "aws_cloudwatch_metric_alarm" "error_level_log_corrupted_alarm" {
  alarm_name                = "WarningErrorLevelLogCorrupted${var.custom_alarm_label}"
  alarm_description         = "PBS error log - Log is corrupted, greater than threshold ${var.error_log_log_corrupted_threshold}"
  comparison_operator       = "GreaterThanThreshold"
  evaluation_periods        = 60
  threshold                 = var.error_log_log_corrupted_threshold
  insufficient_data_actions = []
  metric_name               = "metric_filter_log_corrupted"
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

resource "aws_cloudwatch_metric_alarm" "error_level_missing_transaction_alarm" {
  alarm_name                = "WarningErrorLevelMissingTransaction${var.custom_alarm_label}"
  alarm_description         = "PBS error log - Cannot find the transaction with id, greater than threshold ${var.error_log_missing_transaction_threshold}"
  comparison_operator       = "GreaterThanThreshold"
  evaluation_periods        = 60
  threshold                 = var.error_log_missing_transaction_threshold
  insufficient_data_actions = []
  metric_name               = "metric_filter_missing_transaction"
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

resource "aws_cloudwatch_metric_alarm" "error_level_checkpointing_alarm" {
  alarm_name                = "WarningErrorLevelCheckpointing${var.custom_alarm_label}"
  alarm_description         = "PBS error log - Checkpointing failed, greater than threshold ${var.error_log_checkpointing_threshold}"
  comparison_operator       = "GreaterThanThreshold"
  evaluation_periods        = 60
  threshold                 = var.error_log_checkpointing_threshold
  insufficient_data_actions = []
  metric_name               = "metric_filter_checkpointing"
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

resource "aws_cloudwatch_metric_alarm" "error_level_database_read_alarm" {
  alarm_name                = "WarningErrorLevelDatabaseRead${var.custom_alarm_label}"
  alarm_description         = "PBS error log - Failed to read lease from the database, greater than threshold ${var.error_log_database_read_threshold}"
  comparison_operator       = "GreaterThanThreshold"
  evaluation_periods        = 60
  threshold                 = var.error_log_database_read_threshold
  insufficient_data_actions = []
  metric_name               = "metric_filter_database_read"
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

resource "aws_cloudwatch_metric_alarm" "error_level_database_update_alarm" {
  alarm_name                = "WarningErrorLevelDatabaseUpdate${var.custom_alarm_label}"
  alarm_description         = "PBS error log - Failed to update lease from the database, greater than threshold ${var.error_log_database_update_threshold}"
  comparison_operator       = "GreaterThanThreshold"
  evaluation_periods        = 60
  threshold                 = var.error_log_database_update_threshold
  insufficient_data_actions = []
  metric_name               = "metric_filter_database_update"
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

resource "aws_cloudwatch_metric_alarm" "error_level_missing_component_id_alarm" {
  alarm_name                = "WarningErrorLevelMissingComponentId${var.custom_alarm_label}"
  alarm_description         = "PBS error log - Cannot find the component with id, greater than threshold ${var.error_log_missing_component_id_threshold}"
  comparison_operator       = "GreaterThanThreshold"
  evaluation_periods        = 60
  threshold                 = var.error_log_missing_component_id_threshold
  insufficient_data_actions = []
  metric_name               = "metric_filter_missing_component_id"
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

resource "aws_cloudwatch_metric_alarm" "error_level_handle_journal_alarm" {
  alarm_name                = "WarningErrorLevelHandleJournal${var.custom_alarm_label}"
  alarm_description         = "PBS error log - Cannot handle the journal log with id, greater than threshold ${var.error_log_handle_journal_threshold}"
  comparison_operator       = "GreaterThanThreshold"
  evaluation_periods        = 60
  threshold                 = var.error_log_handle_journal_threshold
  insufficient_data_actions = []
  metric_name               = "metric_filter_handle_journal"
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


# elb_4xx_error_ratio_high(4XXerror/TotalCount)
resource "aws_cloudwatch_metric_alarm" "elb_4xx_error_ratio_high" {
  alarm_name                = "WarningELB4xxErrorRatioHigh${var.custom_alarm_label}"
  comparison_operator       = "GreaterThanOrEqualToThreshold"
  evaluation_periods        = var.eval_period_sec
  threshold                 = var.error_ratio_4xx_threshold
  alarm_description         = "4XXError rate ratio has exceeded ${var.error_ratio_4xx_threshold}%"
  insufficient_data_actions = []
  alarm_actions             = [var.sns_topic_arn]
  ok_actions                = [var.sns_topic_arn]

  tags = {
    environment = var.environment
  }

  metric_query {
    id = "e1"
    # Setting a minimum request count to remove the noise of low qps.
    expression  = "IF(m1>10,(m2/m1)*100,0)"
    label       = "Error Rate Ratio"
    return_data = "true"
  }

  metric_query {
    id = "m1"

    metric {
      metric_name = "RequestCount"
      namespace   = "AWS/ApplicationELB"
      period      = "60"
      stat        = "Sum"
      unit        = "Count"
      dimensions = {
        LoadBalancer = var.loadbalancer
      }
    }
  }

  metric_query {
    id = "m2"

    metric {
      metric_name = "HTTPCode_Target_4XX_Count"
      namespace   = "AWS/ApplicationELB"
      period      = "60"
      stat        = "Sum"
      unit        = "Count"
      dimensions = {
        LoadBalancer = var.loadbalancer
      }
    }
  }
}

# elb_5xx_error_ratio_high(5XXerror/TotalCount)
resource "aws_cloudwatch_metric_alarm" "elb_5xx_error_ratio_high" {
  alarm_name                = "WarningELB5xxErrorRatioHigh${var.custom_alarm_label}"
  comparison_operator       = "GreaterThanOrEqualToThreshold"
  evaluation_periods        = var.eval_period_sec
  threshold                 = var.error_ratio_5xx_threshold
  alarm_description         = "5XXError rate ratio has exceeded ${var.error_ratio_5xx_threshold}%"
  insufficient_data_actions = []
  alarm_actions             = [var.sns_topic_arn]
  ok_actions                = [var.sns_topic_arn]

  tags = {
    environment = var.environment
  }

  metric_query {
    id = "e1"
    # Setting a minimum request count to remove the noise of low qps.
    expression  = "IF(m1>10,(m2/m1)*100,0)"
    label       = "Error Rate Ratio"
    return_data = "true"
  }

  metric_query {
    id = "m1"

    metric {
      metric_name = "RequestCount"
      namespace   = "AWS/ApplicationELB"
      period      = "60"
      stat        = "Sum"
      unit        = "Count"
      dimensions = {
        LoadBalancer = var.loadbalancer
      }
    }
  }

  metric_query {
    id = "m2"

    metric {
      metric_name = "HTTPCode_Target_5XX_Count"
      namespace   = "AWS/ApplicationELB"
      period      = "60"
      stat        = "Sum"
      unit        = "Count"
      dimensions = {
        LoadBalancer = var.loadbalancer
      }
    }
  }
}

resource "aws_cloudwatch_metric_alarm" "budget_key_table_read_capacity_alarm" {
  alarm_name        = "InfoDynamodbBudgetKeyTableReadCapacityRatio${var.custom_alarm_label}"
  alarm_description = "PBS budget key table is reaching its read capacity units limit."

  comparison_operator = "GreaterThanOrEqualToThreshold"
  threshold           = var.budget_key_table_read_capacity_alarm_ratio_threshold

  evaluation_periods = 1

  insufficient_data_actions = []
  treat_missing_data        = "notBreaching"
  alarm_actions             = [var.sns_topic_arn]
  ok_actions                = [var.sns_topic_arn]

  metric_query {
    id          = "e1"
    expression  = "m1/(${var.budget_table_read_capacity}*${var.eval_period_sec})"
    label       = "${var.environment} ${var.budget_key_table_name} Read Capacity Usage Ratio"
    return_data = "true"
  }

  metric_query {
    id = "m1"

    metric {
      metric_name = "ConsumedReadCapacityUnits"
      namespace   = "AWS/DynamoDB"
      period      = var.eval_period_sec
      stat        = "Sum"

      dimensions = {
        TableName = var.budget_key_table_name
      }
    }
  }


  tags = {
    environment = var.environment
  }
}

resource "aws_cloudwatch_metric_alarm" "budget_key_table_write_capacity_alarm" {
  alarm_name        = "InfoDynamodbBudgetKeyTableWriteCapacityRatio${var.custom_alarm_label}"
  alarm_description = "PBS budget key table is reaching its write capacity units limit."

  comparison_operator = "GreaterThanOrEqualToThreshold"
  threshold           = var.budget_key_table_write_capacity_alarm_ratio_threshold

  evaluation_periods = 1

  insufficient_data_actions = []
  treat_missing_data        = "notBreaching"
  alarm_actions             = [var.sns_topic_arn]
  ok_actions                = [var.sns_topic_arn]

  metric_query {
    id          = "e1"
    expression  = "m1/(${var.budget_table_write_capacity}*${var.eval_period_sec})"
    label       = "${var.environment} ${var.budget_key_table_name} Write Capacity Usage Ratio"
    return_data = "true"
  }

  metric_query {
    id = "m1"

    metric {
      metric_name = "ConsumedWriteCapacityUnits"
      namespace   = "AWS/DynamoDB"
      period      = var.eval_period_sec
      stat        = "Sum"

      dimensions = {
        TableName = var.budget_key_table_name
      }
    }
  }

  tags = {
    environment = var.environment
  }
}

resource "aws_cloudwatch_metric_alarm" "partition_lock_table_read_capacity_alarm" {
  alarm_name        = "InfoDynamodbPartitionLockTableReadCapacityRatio${var.custom_alarm_label}"
  alarm_description = "PBS partition lock table is reaching its read capacity units limit."

  comparison_operator = "GreaterThanOrEqualToThreshold"
  threshold           = var.partition_lock_table_read_capacity_alarm_ratio_threshold

  evaluation_periods = 1

  insufficient_data_actions = []
  treat_missing_data        = "notBreaching"
  alarm_actions             = [var.sns_topic_arn]
  ok_actions                = [var.sns_topic_arn]

  metric_query {
    id          = "e1"
    expression  = "m1/(${var.partition_lock_table_read_capacity}*${var.eval_period_sec})"
    label       = "${var.environment} ${var.partition_lock_table_name} Read Capacity Usage Ratio"
    return_data = "true"
  }

  metric_query {
    id = "m1"

    metric {
      metric_name = "ConsumedReadCapacityUnits"
      namespace   = "AWS/DynamoDB"
      period      = var.eval_period_sec
      stat        = "Sum"

      dimensions = {
        TableName = var.partition_lock_table_name
      }
    }
  }

  tags = {
    environment = var.environment
  }
}

resource "aws_cloudwatch_metric_alarm" "partition_lock_table_write_capacity_alarm" {
  alarm_name        = "InfoDynamodbPartitionLockTableWriteCapacityRatio${var.custom_alarm_label}"
  alarm_description = "PBS partition lock table is reaching its write capacity units limit."

  comparison_operator = "GreaterThanOrEqualToThreshold"
  threshold           = var.partition_lock_table_write_capacity_alarm_ratio_threshold

  evaluation_periods = 1

  insufficient_data_actions = []
  treat_missing_data        = "notBreaching"
  alarm_actions             = [var.sns_topic_arn]
  ok_actions                = [var.sns_topic_arn]

  metric_query {
    id          = "e1"
    expression  = "m1/(${var.partition_lock_table_write_capacity}*${var.eval_period_sec})"
    label       = "${var.environment} ${var.partition_lock_table_name} Write Capacity Usage Ratio"
    return_data = "true"
  }

  metric_query {
    id = "m1"

    metric {
      metric_name = "ConsumedWriteCapacityUnits"
      namespace   = "AWS/DynamoDB"
      period      = var.eval_period_sec
      stat        = "Sum"

      dimensions = {
        TableName = var.partition_lock_table_name
      }
    }
  }

  tags = {
    environment = var.environment
  }
}

resource "aws_cloudwatch_metric_alarm" "reporting_origin_table_read_capacity_alarm" {
  alarm_name        = "InfoDynamodbReportingOriginTableReadCapacityRatio${var.custom_alarm_label}"
  alarm_description = "PBS reporting origin table is reaching its read capacity units limit."

  comparison_operator = "GreaterThanOrEqualToThreshold"
  threshold           = var.reporting_origin_table_read_capacity_alarm_ratio_threshold

  evaluation_periods = 1

  insufficient_data_actions = []
  treat_missing_data        = "notBreaching"
  alarm_actions             = [var.sns_topic_arn]
  ok_actions                = [var.sns_topic_arn]

  metric_query {
    id          = "e1"
    expression  = "m1/(${var.auth_table_read_max_capacity}*${var.eval_period_sec})"
    label       = "${var.environment} ${var.reporting_origin_table_name} Read Capacity Usage Ratio"
    return_data = "true"
  }

  metric_query {
    id = "m1"

    metric {
      metric_name = "ConsumedReadCapacityUnits"
      namespace   = "AWS/DynamoDB"
      period      = var.eval_period_sec
      stat        = "Sum"

      dimensions = {
        TableName = var.reporting_origin_table_name
      }
    }
  }

  tags = {
    environment = var.environment
  }
}

resource "aws_cloudwatch_metric_alarm" "reporting_origin_table_write_capacity_alarm" {
  alarm_name        = "InfoDynamodbReportingOriginTableWriteCapacityRatio${var.custom_alarm_label}"
  alarm_description = "PBS reporting origin table is reaching its write capacity units limit."

  comparison_operator = "GreaterThanOrEqualToThreshold"
  threshold           = var.reporting_origin_table_write_capacity_alarm_ratio_threshold

  evaluation_periods = 1

  insufficient_data_actions = []
  treat_missing_data        = "notBreaching"
  alarm_actions             = [var.sns_topic_arn]
  ok_actions                = [var.sns_topic_arn]

  metric_query {
    id          = "e1"
    expression  = "m1/(${var.auth_table_write_max_capacity}*${var.eval_period_sec})"
    label       = "${var.environment} ${var.reporting_origin_table_name} Write Capacity Usage Ratio"
    return_data = "true"
  }

  metric_query {
    id = "m1"

    metric {
      metric_name = "ConsumedWriteCapacityUnits"
      namespace   = "AWS/DynamoDB"
      period      = var.eval_period_sec
      stat        = "Sum"

      dimensions = {
        TableName = var.reporting_origin_table_name
      }
    }
  }

  tags = {
    environment = var.environment
  }
}

resource "aws_cloudwatch_metric_alarm" "pbs_authorization_v2_table_read_capacity_alarm" {
  alarm_name        = "InfoDynamodbPBSAuthorizationV2TableReadCapacityRatio${var.custom_alarm_label}"
  alarm_description = "PBS authorization V2 table is reaching its read capacity units limit."

  comparison_operator = "GreaterThanOrEqualToThreshold"
  threshold           = var.pbs_authorization_v2_table_read_capacity_alarm_ratio_threshold

  evaluation_periods = 1

  insufficient_data_actions = []
  treat_missing_data        = "notBreaching"
  alarm_actions             = [var.sns_topic_arn]
  ok_actions                = [var.sns_topic_arn]

  metric_query {
    id          = "e1"
    expression  = "m1/(${var.pbs_authorization_v2_table_read_max_capacity}*${var.eval_period_sec})"
    label       = "${var.environment} ${var.pbs_authorization_v2_table_name} Read Capacity Usage Ratio"
    return_data = "true"
  }

  metric_query {
    id = "m1"

    metric {
      metric_name = "ConsumedReadCapacityUnits"
      namespace   = "AWS/DynamoDB"
      period      = var.eval_period_sec
      stat        = "Sum"

      dimensions = {
        TableName = var.pbs_authorization_v2_table_name
      }
    }
  }

  tags = {
    environment = var.environment
  }
}

resource "aws_cloudwatch_metric_alarm" "pbs_authorization_v2_table_write_capacity_alarm" {
  alarm_name        = "InfoDynamodbPBSAuthorizationV2TableWriteCapacityRatio${var.custom_alarm_label}"
  alarm_description = "PBS authorization V2 table is reaching its write capacity units limit."

  comparison_operator = "GreaterThanOrEqualToThreshold"
  threshold           = var.pbs_authorization_v2_table_write_capacity_alarm_ratio_threshold

  evaluation_periods = 1

  insufficient_data_actions = []
  treat_missing_data        = "notBreaching"
  alarm_actions             = [var.sns_topic_arn]
  ok_actions                = [var.sns_topic_arn]

  metric_query {
    id          = "e1"
    expression  = "m1/(${var.pbs_authorization_v2_table_write_max_capacity}*${var.eval_period_sec})"
    label       = "${var.environment} ${var.pbs_authorization_v2_table_name} Write Capacity Usage Ratio"
    return_data = "true"
  }

  metric_query {
    id = "m1"

    metric {
      metric_name = "ConsumedWriteCapacityUnits"
      namespace   = "AWS/DynamoDB"
      period      = var.eval_period_sec
      stat        = "Sum"

      dimensions = {
        TableName = var.pbs_authorization_v2_table_name
      }
    }
  }

  tags = {
    environment = var.environment
  }
}