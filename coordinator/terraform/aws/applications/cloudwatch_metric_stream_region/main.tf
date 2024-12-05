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
      source = "hashicorp/aws"
      # 4.16 added cloudwatch_metric_stream statistics_configuration
      # 4.67 (the last v4) added include_filter metric_name
      # 5.32 adjusts buffer interval to allow below 60s.
      # 5.59 firehose supports secret managed config
      version = ">= 5.59"
    }
  }
}

data "aws_caller_identity" "current" {}
data "aws_region" "current" {}

locals {
  # If modifying any of these names, update wildcards in cloudwatch_metric_stream_iam module.
  metric_stream_name   = "${var.environment}-metric-stream"
  firehose_stream_name = "${local.metric_stream_name}-firehose"
  # All lowercase, with account ID for uniqueness.
  # https://docs.aws.amazon.com/AmazonS3/latest/userguide/bucketnamingrules.html
  s3_bucket_name = "${local.metric_stream_name}-${data.aws_region.current.name}-${data.aws_caller_identity.current.account_id}"
}
