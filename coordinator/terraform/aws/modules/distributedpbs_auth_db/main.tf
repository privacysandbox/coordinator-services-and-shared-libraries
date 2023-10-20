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
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 3.0"
    }
  }
}

# Note: The below tfsec issues are ignored because DynamoDB uses encryption
# by default.
#tfsec:ignore:aws-dynamodb-enable-at-rest-encryption
#tfsec:ignore:aws-dynamodb-table-customer-key
resource "aws_dynamodb_table" "auth_table" {
  name           = "${var.environment_prefix}-google-scp-pbs-reporting-origin-auth"
  billing_mode   = "PROVISIONED"
  read_capacity  = var.auth_table_read_initial_capacity
  write_capacity = var.auth_table_write_initial_capacity

  lifecycle {
    ignore_changes = [
      # Ignore changes in read/write capacity since we have an auto-scaling policy
      read_capacity,
      write_capacity
    ]
  }

  hash_key = "iam_role"

  attribute {
    name = "iam_role"
    type = "S"
  }

  point_in_time_recovery {
    enabled = var.auth_table_enable_point_in_time_recovery
  }

  tags = {
    Name        = "${var.environment_prefix}-google-scp-pbs-budget-keys"
    Environment = "${var.environment_prefix}"
  }
}

resource "aws_appautoscaling_target" "table_read" {
  min_capacity       = var.auth_table_read_initial_capacity
  max_capacity       = var.auth_table_read_max_capacity
  resource_id        = "table/${aws_dynamodb_table.auth_table.name}"
  scalable_dimension = "dynamodb:table:ReadCapacityUnits"
  service_namespace  = "dynamodb"
}

resource "aws_appautoscaling_policy" "table_read_policy" {
  name               = "DynamoDBReadCapacityUtilization:${aws_appautoscaling_target.table_read.resource_id}"
  policy_type        = "TargetTrackingScaling"
  resource_id        = aws_appautoscaling_target.table_read.resource_id
  scalable_dimension = aws_appautoscaling_target.table_read.scalable_dimension
  service_namespace  = aws_appautoscaling_target.table_read.service_namespace

  target_tracking_scaling_policy_configuration {
    predefined_metric_specification {
      predefined_metric_type = "DynamoDBReadCapacityUtilization"
    }

    target_value = var.auth_table_read_scale_utilization
  }
}

resource "aws_appautoscaling_target" "table_write" {
  min_capacity       = var.auth_table_write_initial_capacity
  max_capacity       = var.auth_table_write_max_capacity
  resource_id        = "table/${aws_dynamodb_table.auth_table.name}"
  scalable_dimension = "dynamodb:table:WriteCapacityUnits"
  service_namespace  = "dynamodb"
}

resource "aws_appautoscaling_policy" "table_write_policy" {
  name               = "DynamoDBWriteCapacityUtilization:${aws_appautoscaling_target.table_write.resource_id}"
  policy_type        = "TargetTrackingScaling"
  resource_id        = aws_appautoscaling_target.table_write.resource_id
  scalable_dimension = aws_appautoscaling_target.table_write.scalable_dimension
  service_namespace  = aws_appautoscaling_target.table_write.service_namespace

  target_tracking_scaling_policy_configuration {
    predefined_metric_specification {
      predefined_metric_type = "DynamoDBWriteCapacityUtilization"
    }

    target_value = var.auth_table_write_scale_utilization
  }
}

resource "aws_dynamodb_table" "pbs_authorization_table_v2" {
  name           = "${var.environment_prefix}-pbs-authorization-v2"
  billing_mode   = "PROVISIONED"
  read_capacity  = var.auth_table_read_initial_capacity
  write_capacity = var.auth_table_write_initial_capacity

  lifecycle {
    ignore_changes = [
      # Ignore changes in read/write capacity since we have an auto-scaling policy
      read_capacity,
      write_capacity
    ]
  }

  hash_key = "adtech_identity"

  attribute {
    name = "adtech_identity"
    type = "S"
  }

  point_in_time_recovery {
    enabled = var.auth_table_enable_point_in_time_recovery
  }

  tags = {
    Name        = "${var.environment_prefix}-google-scp-pbs-budget-keys"
    Environment = "${var.environment_prefix}"
  }
}

resource "aws_appautoscaling_target" "pbs_authorization_table_v2_read_target" {
  min_capacity       = var.auth_table_read_initial_capacity
  max_capacity       = var.auth_table_read_max_capacity
  resource_id        = "table/${aws_dynamodb_table.pbs_authorization_table_v2.name}"
  scalable_dimension = "dynamodb:table:ReadCapacityUnits"
  service_namespace  = "dynamodb"
}

resource "aws_appautoscaling_policy" "pbs_authorization_table_v2_read_policy" {
  name               = "DynamoDBReadCapacityUtilization:${aws_appautoscaling_target.pbs_authorization_table_v2_read_target.resource_id}"
  policy_type        = "TargetTrackingScaling"
  resource_id        = aws_appautoscaling_target.pbs_authorization_table_v2_read_target.resource_id
  scalable_dimension = aws_appautoscaling_target.pbs_authorization_table_v2_read_target.scalable_dimension
  service_namespace  = aws_appautoscaling_target.pbs_authorization_table_v2_read_target.service_namespace

  target_tracking_scaling_policy_configuration {
    predefined_metric_specification {
      predefined_metric_type = "DynamoDBReadCapacityUtilization"
    }

    target_value = var.auth_table_read_scale_utilization
  }
}

resource "aws_appautoscaling_target" "pbs_authorization_table_v2_write_target" {
  min_capacity       = var.auth_table_write_initial_capacity
  max_capacity       = var.auth_table_write_max_capacity
  resource_id        = "table/${aws_dynamodb_table.pbs_authorization_table_v2.name}"
  scalable_dimension = "dynamodb:table:WriteCapacityUnits"
  service_namespace  = "dynamodb"
}

resource "aws_appautoscaling_policy" "pbs_authorization_table_v2_write_policy" {
  name               = "DynamoDBWriteCapacityUtilization:${aws_appautoscaling_target.pbs_authorization_table_v2_write_target.resource_id}"
  policy_type        = "TargetTrackingScaling"
  resource_id        = aws_appautoscaling_target.pbs_authorization_table_v2_write_target.resource_id
  scalable_dimension = aws_appautoscaling_target.pbs_authorization_table_v2_write_target.scalable_dimension
  service_namespace  = aws_appautoscaling_target.pbs_authorization_table_v2_write_target.service_namespace

  target_tracking_scaling_policy_configuration {
    predefined_metric_specification {
      predefined_metric_type = "DynamoDBWriteCapacityUtilization"
    }

    target_value = var.auth_table_write_scale_utilization
  }
}