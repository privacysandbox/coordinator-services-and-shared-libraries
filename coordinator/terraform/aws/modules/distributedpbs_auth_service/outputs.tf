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

output "api_gateway_base_url" {
  description = "Base URL for API Gateway"
  value       = local.output_domain
}

output "api_gateway_id" {
  description = "ID of the API Gateway"
  value       = aws_apigatewayv2_api.lambda_api.id
}

output "api_gateway_arn" {
  description = "ARN of the API Gateway stage that provides auth"
  value       = "${aws_apigatewayv2_stage.api_gateway_stage.execution_arn}/POST/auth"
}
