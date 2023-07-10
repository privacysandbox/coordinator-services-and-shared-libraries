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

variable "account_id" {
  description = "Current account ID"
  type        = string
}

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production."
  type        = string
}

variable "region" {
  description = "Region where key hosting services are hosted."
  type        = string
}

variable "get_encryption_key_lambda_version" {
  description = "Lambda version used with provisioned concurrency in primary region"
  type        = string
}

variable "unified_keyhosting_dashboard_time_period_seconds" {
  description = "Time period that acts as window for dashboard metrics. Measured in seconds."
  type        = number
}

variable "unified_keyhosting_api_gateway_id" {
  description = "API Gateway ApiId that host the private keyhosting API."
  type        = string
}
