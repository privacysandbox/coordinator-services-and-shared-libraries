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

module "api_gateway_alarms" {
  count               = var.api_gateway_alarms_enabled ? 1 : 0
  source              = "../../monitoring/common/api_gateway_alarms"
  api_name            = var.name
  environment         = var.environment
  api_gateway_id      = aws_apigatewayv2_api.api_gateway.id
  sns_topic_arn       = var.api_gateway_sns_topic_arn
  eval_period_sec     = var.api_gateway_alarm_eval_period_sec
  max_latency_ms      = var.api_gateway_api_max_latency_ms
  error_5xx_threshold = var.api_gateway_5xx_threshold
  custom_alarm_label  = var.custom_alarm_label
}
