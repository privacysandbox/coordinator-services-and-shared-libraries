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

variable "environment_prefix" {
  type     = string
  nullable = false
}

variable "beanstalk_app_bucket_force_destroy" {
  description = "Whether to destroy the bucket even if it is not empty."
  type        = bool
  default     = false
  nullable    = false
}

variable "pbs_aws_lb_arn" {
  description = "Elastic Load Balancing account ARN to grant ELB access log policy permission."
  type        = map(string)
}

variable "aws_region" {
  type     = string
  nullable = false
}

variable "aws_account_id" {
  type     = string
  nullable = false
}
