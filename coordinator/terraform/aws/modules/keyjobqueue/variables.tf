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

variable "environment" {
  description = "Description for the environment, e.g. dev, staging, production"
  type        = string
}

variable "enable_key_rotation" {
  description = "Whether to enable the CloudWatch event for key rotation."
  type        = bool
}

variable "key_job_queue_name" {
  description = "Name of the SQS queue to create for key jobs. Must end in .fifo"
  type        = string

  # Validate that the name ends in .fifo which is required for FIFO queues
  validation {
    condition     = can(regex("^.+\\.fifo$", var.key_job_queue_name))
    error_message = "Queue name must end in '.fifo'."
  }
}
