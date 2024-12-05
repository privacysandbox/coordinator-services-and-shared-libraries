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

terraform {
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = ">= 5.59"
    }
  }
}

data "aws_caller_identity" "current" {}
data "aws_region" "current" {}

locals {
  metric_stream_name   = "${var.environment}-metric-stream"
  firehose_stream_name = "${local.metric_stream_name}-firehose"

  # Wildcards are used in place of regions, to support instantiating module
  # cloudwatch_metric_stream_region in as many regions as necessary.
  firehose_stream_arn    = "arn:aws:firehose:*:${data.aws_caller_identity.current.account_id}:deliverystream/${local.firehose_stream_name}"
  firehose_backup_s3_arn = "arn:aws:s3:::${local.metric_stream_name}-*-${data.aws_caller_identity.current.account_id}"
  firehose_secret_arn    = "arn:aws:secretsmanager:*:${data.aws_caller_identity.current.account_id}:secret:${local.firehose_stream_name}-secret-*"
}
