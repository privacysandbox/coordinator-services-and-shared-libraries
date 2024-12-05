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

output "firehose_role_arn" {
  value       = aws_iam_role.firehose_stream.arn
  description = "IAM Role ARN for Firehose to assume. Produced by cloudwatch_metric_stream_iam module."
}

output "metric_stream_role_arn" {
  value       = aws_iam_role.metric_stream.arn
  description = "IAM Role ARN for CloudWatch Metric Stream to assume. Produced by cloudwatch_metric_stream_iam module."
}
