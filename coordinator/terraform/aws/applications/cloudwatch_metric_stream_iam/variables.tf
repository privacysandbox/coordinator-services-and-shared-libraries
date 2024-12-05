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

variable "environment" {
  type        = string
  description = "Prefix for the environment, e.g. \"a\" for Type-1, \"b\" for Type-2."
  nullable    = false
}

variable "service_account_email" {
  type        = string
  description = "The email of the Google Cloud Service Account that the CloudWatch Stream Receiver application is running as."
  nullable    = false
}
