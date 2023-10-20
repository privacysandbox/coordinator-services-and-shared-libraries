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
  description = "Description for the environment, e.g. dev, staging, production."
  type        = string
}

variable "region" {
  description = "Region where key storage services are hosted."
  type        = string
}

variable "key_storage_api_gateway_id" {
  description = "API Gateway ApiId to create a key storage API dashboard for."
  type        = string
}

variable "get_data_key_lambda_version" {
  description = "Get Data Key Lambda version used with provisioned concurrency in primary region"
  type        = string
}

variable "create_key_lambda_version" {
  description = "Create Key Lambda version used with provisioned concurrency in primary region"
  type        = string
}

variable "key_storage_dashboard_time_period_seconds" {
  description = "Time period that acts as window for dashboard metrics. Measured in seconds."
  type        = number
}
