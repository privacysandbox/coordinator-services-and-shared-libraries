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

variable "account_id" {
  description = "Current account ID"
  type        = string
}
variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production"
  type        = string
}
variable "primary_region" {
  description = "Region where all services will be created. Serves as primary region for keydb and primary origin for cloudfront"
  type        = string
}

variable "secondary_region" {
  description = "Region where public keyhosting service is replicated. Serves as failover region for cloudfront"
  type        = string
}

variable "cloudfront_id" {
  description = "Cloudfront Distribution Id for Public Key Hosting Service"
  type        = string
}
variable "get_public_key_primary_api_gateway_id" {
  description = "API Gateway ApiId in primary region"
  type        = string
}
variable "get_public_key_primary_get_public_key_lambda_version" {
  description = "Lambda version used with provisioned concurrency in primary region"
  type        = string
}

variable "get_public_key_secondary_api_gateway_id" {
  description = "API Gateway ApiId in secondary region"
  type        = string
}
variable "get_public_key_secondary_get_public_key_lambda_version" {
  description = "Lambda version used with provisioned concurrency in secondary region"
  type        = string
}

variable "get_public_key_dashboard_time_period_seconds" {
  description = "Time period that acts as window for dashboard metrics. Measured in seconds."
  type        = number
}
