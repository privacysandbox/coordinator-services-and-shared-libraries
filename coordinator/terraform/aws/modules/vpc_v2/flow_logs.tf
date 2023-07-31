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

locals {
  resource_name_prefix = "${var.environment}-vpc-flow-logs"
  tags                 = var.vpc_default_tags
}

data "aws_iam_policy_document" "assume_role" {
  count = var.enable_flow_logs ? 1 : 0
  statement {
    effect = "Allow"

    principals {
      type        = "Service"
      identifiers = ["vpc-flow-logs.amazonaws.com"]
    }

    actions = ["sts:AssumeRole"]
  }
}

resource "aws_iam_role" "flow_log_role" {
  count              = var.enable_flow_logs ? 1 : 0
  name_prefix        = local.resource_name_prefix
  assume_role_policy = data.aws_iam_policy_document.assume_role[0].json
  tags               = local.tags
}

data "aws_iam_policy_document" "allow_logging" {
  count = var.enable_flow_logs ? 1 : 0
  statement {
    effect = "Allow"

    actions = [
      "logs:CreateLogGroup",
      "logs:CreateLogStream",
      "logs:PutLogEvents",
      "logs:DescribeLogGroups",
      "logs:DescribeLogStreams"
    ]

    resources = ["*"]
  }
}

resource "aws_iam_role_policy" "allow_flow_log_policy" {
  count       = var.enable_flow_logs ? 1 : 0
  name_prefix = local.resource_name_prefix
  role        = aws_iam_role.flow_log_role[0].id
  policy      = data.aws_iam_policy_document.allow_logging[0].json
}

resource "aws_cloudwatch_log_group" "flow_log_group" {
  count             = var.enable_flow_logs ? 1 : 0
  name_prefix       = "/aws/vendedlogs/vpc/${local.resource_name_prefix}"
  retention_in_days = var.flow_logs_retention_in_days
  tags              = local.tags
}

resource "aws_flow_log" "vpc_flow_logs" {
  count           = var.enable_flow_logs ? 1 : 0
  iam_role_arn    = aws_iam_role.flow_log_role[0].arn
  log_destination = aws_cloudwatch_log_group.flow_log_group[0].arn
  traffic_type    = var.flow_logs_traffic_type
  vpc_id          = aws_vpc.main.id
}
