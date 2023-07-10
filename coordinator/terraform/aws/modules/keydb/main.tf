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

data "aws_region" "current" {}

terraform {
  required_version = "~> 1.2.3"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 3.0"
    }
  }
}

resource "aws_dynamodb_table" "keydb" {
  name = var.table_name

  billing_mode     = "PROVISIONED"
  read_capacity    = var.read_capacity
  write_capacity   = var.write_capacity
  stream_enabled   = true
  stream_view_type = "NEW_AND_OLD_IMAGES"

  server_side_encryption {
    enabled = true
  }

  hash_key = "KeyId"
  attribute {
    name = "KeyId"
    type = "S"
  }

  point_in_time_recovery {
    enabled = var.enable_dynamo_point_in_time_recovery
  }

  # Underlying DynamoDB API that terraform uses returns exception if trying to set TTL = false to a table
  # that already has TTL set to false. Dynamic block removes entire block if enable_keydb_ttl = false
  # so terraform does not try to update DynamoDB. *Note, due the API, this does not allow TTL to be set
  # to false after being set to true. TTL can be kept at false, kept at true, or changed from false to true.
  # https://github.com/hashicorp/terraform-provider-aws/issues/10304
  dynamic "ttl" {
    # Dynamic only has for_each, so if enable_keydb_ttl = true, then use dummy list.
    for_each = var.enable_keydb_ttl ? ["unused"] : []
    content {
      attribute_name = "TtlTime"
      enabled        = true
    }
  }

  dynamic "replica" {
    for_each = var.keydb_replica_regions
    content {
      region_name = replica.value
    }
  }

  tags = {
    name        = var.table_name
    service     = "key-hosting-service"
    environment = var.environment
    role        = "keydb"
  }
}

# Autoscaling is necessary for replication
resource "aws_appautoscaling_target" "keydb_read_target" {
  max_capacity       = var.autoscaling_read_max_capacity
  min_capacity       = var.read_capacity
  resource_id        = "table/${aws_dynamodb_table.keydb.name}"
  scalable_dimension = "dynamodb:table:ReadCapacityUnits"
  service_namespace  = "dynamodb"
}

resource "aws_appautoscaling_policy" "keydb_read_policy" {
  name               = "dynamodb-read-capacity-utilization-${aws_appautoscaling_target.keydb_read_target.resource_id}"
  policy_type        = "TargetTrackingScaling"
  resource_id        = aws_appautoscaling_target.keydb_read_target.resource_id
  scalable_dimension = aws_appautoscaling_target.keydb_read_target.scalable_dimension
  service_namespace  = aws_appautoscaling_target.keydb_read_target.service_namespace

  target_tracking_scaling_policy_configuration {
    predefined_metric_specification {
      predefined_metric_type = "DynamoDBReadCapacityUtilization"
    }
    target_value = var.autoscaling_read_target_percentage
  }
}

resource "aws_appautoscaling_target" "keydb_write_target" {
  max_capacity       = var.autoscaling_write_max_capacity
  min_capacity       = var.write_capacity
  resource_id        = "table/${aws_dynamodb_table.keydb.name}"
  scalable_dimension = "dynamodb:table:WriteCapacityUnits"
  service_namespace  = "dynamodb"
}

resource "aws_appautoscaling_policy" "keydb_write_policy" {
  name               = "dynamodb-write-capacity-utilization-${aws_appautoscaling_target.keydb_write_target.resource_id}"
  policy_type        = "TargetTrackingScaling"
  resource_id        = aws_appautoscaling_target.keydb_write_target.resource_id
  scalable_dimension = aws_appautoscaling_target.keydb_write_target.scalable_dimension
  service_namespace  = aws_appautoscaling_target.keydb_write_target.service_namespace

  target_tracking_scaling_policy_configuration {
    predefined_metric_specification {
      predefined_metric_type = "DynamoDBWriteCapacityUtilization"
    }
    target_value = var.autoscaling_write_target_percentage
  }
}
