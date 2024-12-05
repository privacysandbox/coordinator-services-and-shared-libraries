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

data "aws_iam_policy_document" "metric_stream_assume" {
  statement {
    actions = ["sts:AssumeRole"]

    principals {
      identifiers = ["streams.metrics.cloudwatch.amazonaws.com"]
      type        = "Service"
    }
  }
}

data "aws_iam_policy_document" "metric_stream_policy" {
  statement {
    actions = [
      "firehose:PutRecord",
      "firehose:PutRecordBatch",
    ]
    resources = [local.firehose_stream_arn]
  }
}

resource "aws_iam_role" "metric_stream" {
  name               = "${local.metric_stream_name}-role"
  assume_role_policy = data.aws_iam_policy_document.metric_stream_assume.json
}

resource "aws_iam_role_policy" "metric_stream" {
  name   = "${local.metric_stream_name}-policy"
  role   = aws_iam_role.metric_stream.id
  policy = data.aws_iam_policy_document.metric_stream_policy.json
}
