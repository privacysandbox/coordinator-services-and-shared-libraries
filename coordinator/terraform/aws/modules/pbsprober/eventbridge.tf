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
  eventbridge_cron_name = "${var.environment}_pbsprober_cron"
}

resource "aws_cloudwatch_event_rule" "pbsprober_cron" {
  name                = local.eventbridge_cron_name
  description         = "Cron to run the PBS Prober Lambda"
  schedule_expression = "rate(1 minute)"
}

resource "aws_cloudwatch_event_target" "pbsprober_cron_target" {
  target_id = "${local.eventbridge_cron_name}-target"
  rule      = aws_cloudwatch_event_rule.pbsprober_cron.name
  arn       = aws_lambda_function.pbsprober_lambda.arn
  retry_policy {
    maximum_retry_attempts       = 0
    maximum_event_age_in_seconds = 60
  }
  depends_on = [
    aws_cloudwatch_event_rule.pbsprober_cron,
    aws_lambda_function.pbsprober_lambda,
  ]
}

resource "aws_lambda_permission" "pbsprober_lambda_permission" {
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.pbsprober_lambda.function_name
  principal     = "events.amazonaws.com"
  source_arn    = aws_cloudwatch_event_rule.pbsprober_cron.arn
  depends_on = [
    aws_cloudwatch_event_rule.pbsprober_cron,
    aws_lambda_function.pbsprober_lambda,
  ]
}
